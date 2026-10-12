/**
 * ============================================================================
 * ULTRASUONO - Audio Effects Processor Implementation
 * ============================================================================
 * 
 * File: src/AudioProcessor.cpp
 * 
 * Implementazione del processamento audio
 * 
 * ============================================================================
 */

#include "AudioProcessor.h"
#include <cmath>

DSY_SDRAM_BSS static daisysp::ReverbSc g_reverb;


namespace
{
float Clamp01(float value)
{
    if(value < 0.0f)
        return 0.0f;
    if(value > 1.0f)
        return 1.0f;
    return value;
}

float SmoothStep(float edge0, float edge1, float x)
{
    float t = Clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

float EqualPowerMix(float dry, float wet, float amount)
{
    if(amount <= 0.0f) return dry;
    if(amount >= 1.0f) return wet;
    float a = SmoothStep(0.0f, 1.0f, amount);
    return dry * (1.0f - a) + wet * a;
}

inline float FastLinearMix(float dry, float wet, float amount)
{
    if(amount <= 0.0f) return dry;
    if(amount >= 1.0f) return wet;
    return dry + (wet - dry) * amount;
}

float SoftClipSymmetric(float x)
{
    float ax = std::fabs(x);
    return x / (1.0f + ax);
}

float SanitizeFinite(float x)
{
    return std::isfinite(x) ? x : 0.0f;
}

float HardClip(float x, float threshold)
{
    if(x > threshold)
        return threshold;
    if(x < -threshold)
        return -threshold;
    return x;
}

float TubeSaturation(float x, float drive)
{
    // CPU-safe approximation of atan saturation for embedded callback.
    float y = x * drive;
    return y / (1.0f + std::fabs(y));
}

float DiodeClip(float x, float threshold)
{
    // Replace exp() branches with a lighter soft-knee diode-like curve.
    if(x > threshold)
    {
        float over = x - threshold;
        return threshold + over / (1.0f + 2.5f * over);
    }
    if(x < -threshold)
    {
        float over = -threshold - x;
        return -threshold - over / (1.0f + 2.5f * over);
    }
    return x;
}

float StageValue(float amount, float min_value, float max_value)
{
    float a = Clamp01(amount);
    if(a <= 0.0f)
    {
        return 0.0f;
    }
    return min_value + (max_value - min_value) * a;
}

float RangeAmount(float value,
                  float start,
                  float end,
                  bool  reverse = false,
                  bool  keep_after_end = true)
{
    if(value > end && !keep_after_end)
    {
        return 0.0f;
    }

    float amount = SmoothStep(start, end, value);
    return reverse ? (1.0f - amount) : amount;
}

float SensorToPercent(float value)
{
    float percent = (value <= 1.0f) ? (value * 100.0f) : value;
    if(percent < 0.0f)
        return 0.0f;
    if(percent > 100.0f)
        return 100.0f;
    return percent;
}

}

namespace
{
constexpr size_t kDryGateBands = 3u;  // 3 bands for CPU optimization
// Punti di crossover/analisi del gate secco: modifica questi valori per
// spostare le 3 bande senza toccare la logica di envelope o smoothing.
constexpr float kDryGateBandFreqs[kDryGateBands] = {
    500.0f, 2500.0f, 2500.0f};
// Soglia base per banda: valori più alti chiudono prima la banda corrispondente.
constexpr float kDryGateBandThresholds[kDryGateBands] = {
    0.045f, 0.038f, 0.026f};
// De-click shaping: basse più morbide per evitare thump/click da gain switching.
constexpr float kDryGateBandOnsetMul[kDryGateBands] = {
    1.60f, 1.38f, 1.08f};
// Quanto silenzio c'è davvero tra i transienti 
constexpr float kDryGateBandClosedGain[kDryGateBands] = { 
    0.50f, 0.30f, 0.020f};
// Quanto velocemente si apre la porta: più veloce per le basse per evitare di tagliare i transienti.
constexpr float kDryGateBandDriveWidth[kDryGateBands] = {
    1.55f, 1.25f, 0.780f};
// Quanto velocemente si asciuga dopo l'attacco'
constexpr float kDryGateBandOpenStepScale[kDryGateBands] = { 
    0.160f, 0.280f, 1.20f};
// Quanto velocemente si chiude dopo il rilascio: più veloce per le alte per evitare di lasciare passare troppo sustain.
constexpr float kDryGateBandCloseStepScale[kDryGateBands] = {
    0.080f, 0.165f, 1.00f};

static daisysp::Svf g_dry_gate_filters_l[kDryGateBands];
static daisysp::Svf g_dry_gate_filters_r[kDryGateBands];
static float g_dry_gate_env_l[kDryGateBands];
static float g_dry_gate_env_r[kDryGateBands];
static float g_dry_gate_slow_env_l[kDryGateBands];
static float g_dry_gate_slow_env_r[kDryGateBands];
static float g_dry_gate_gain_l[kDryGateBands];
static float g_dry_gate_gain_r[kDryGateBands];
static float g_dry_gate_hold_l[kDryGateBands];
static float g_dry_gate_hold_r[kDryGateBands];

static float g_dry_gate_makeup = 1.0f;
static float g_dry_gate_hysteresis = 1.00f;
static float g_dry_gate_env_attack_coeff = 0.0f;
static float g_dry_gate_env_release_coeff = 0.0f;
static float g_dry_gate_slow_env_coeff = 0.0f;
static float g_dry_gate_gain_open_coeff = 0.0f;
static float g_dry_gate_gain_close_coeff = 0.0f;
static float g_dry_gate_hold_samples = 0.0f;
static float g_dry_gate_hold_inv = 0.0f;
static float g_dry_gate_open_threshold[kDryGateBands];
static float g_dry_gate_open_coeff[kDryGateBands];
static float g_dry_gate_close_coeff[kDryGateBands];
static float g_dry_gate_onset_ratio = 1.40f;
static float g_dry_gate_onset_floor = 0.0018f;

inline float ShapeDryGateDrive(size_t band_index, float drive)
{
    drive = Clamp01(drive);
    if(band_index == 0u)
    {
        // Low band: softer opening without expensive powf.
        return sqrtf(drive);
    }
    if(band_index == 1u)
    {
        // Mid band: in-between curve using a cheap sqrt blend.
        float s = sqrtf(drive);
        return drive * (0.58f + 0.42f * s);
    }

    // High band: slightly harder response.
    return drive * (0.55f + 0.45f * drive);
}

inline void UpdateDryGateRuntime(float sample_rate, float amount)
{
    float clamped = Clamp01(amount);
    static float last_amount = -1.0f;
    static float last_sample_rate = 0.0f;
    if(clamped == last_amount && sample_rate == last_sample_rate)
        return;
    last_amount = clamped;
    last_sample_rate = sample_rate;
    float env_attack_ms = 0.55f + (0.14f - 0.55f) * clamped;
    float env_release_ms = 2.4f + (0.35f - 2.4f) * clamped;
    float slow_env_ms = 24.0f + (9.0f - 24.0f) * clamped;
    float gate_open_ms = 0.95f + (0.22f - 0.95f) * clamped;
    float gate_close_ms = 2.75f + (0.50f - 2.75f) * clamped;
    float hold_ms = 18.0f + (7.0f - 18.0f) * clamped;

    g_dry_gate_env_attack_coeff = expf(-1.0f / (0.001f * env_attack_ms * sample_rate));
    g_dry_gate_env_release_coeff = expf(-1.0f / (0.001f * env_release_ms * sample_rate));
    g_dry_gate_slow_env_coeff = expf(-1.0f / (0.001f * slow_env_ms * sample_rate));
    g_dry_gate_gain_open_coeff = expf(-1.0f / (0.001f * gate_open_ms * sample_rate));
    g_dry_gate_gain_close_coeff = expf(-1.0f / (0.001f * gate_close_ms * sample_rate));
    g_dry_gate_hold_samples = hold_ms * 0.001f * sample_rate;

    g_dry_gate_makeup = 1.00f + (1.06f - 1.00f) * clamped;
    g_dry_gate_hysteresis = 1.02f + (1.00f - 1.02f) * clamped;
    g_dry_gate_onset_ratio = 1.55f + (1.32f - 1.55f) * clamped;
    g_dry_gate_onset_floor = 0.0023f + (0.0012f - 0.0023f) * clamped;
    g_dry_gate_hold_inv = g_dry_gate_hold_samples > 1.0f
                             ? 1.0f / g_dry_gate_hold_samples : 0.0f;
    for(size_t i = 0; i < kDryGateBands; ++i)
    {
        g_dry_gate_open_threshold[i] = kDryGateBandThresholds[i] * g_dry_gate_hysteresis + 1.0e-6f;
        g_dry_gate_open_coeff[i] = 1.0f - (1.0f - g_dry_gate_gain_open_coeff) * kDryGateBandOpenStepScale[i];
        g_dry_gate_close_coeff[i] = 1.0f - (1.0f - g_dry_gate_gain_close_coeff) * kDryGateBandCloseStepScale[i];
    }
}

inline float ProcessDryMultibandGate(float in,
                                     daisysp::Svf* filters,
                                     float* band_fast_env,
                                     float* band_slow_env,
                                     float* band_gain,
                                     float* band_hold)
{
    float out = 0.0f;
    for(size_t i = 0; i < kDryGateBands; ++i)
    {
        // Mid/high use identical SVF settings; reuse its two outputs.
        const size_t filter_index = (i == 2u && kDryGateBandFreqs[2] == kDryGateBandFreqs[1])
                                        ? 1u : i;
        if(filter_index == i)
            filters[i].Process(in);
        float band = 0.0f;
        if(i == 0u)
            band = filters[filter_index].Low();
        else if(i == (kDryGateBands - 1u))
            band = filters[filter_index].High();
        else
            band = filters[filter_index].Band();

        float level = fabsf(band);
        float fast_coeff = (level > band_fast_env[i]) ? g_dry_gate_env_attack_coeff
                                                      : g_dry_gate_env_release_coeff;
        band_fast_env[i] = fast_coeff * band_fast_env[i] + (1.0f - fast_coeff) * level;
        band_slow_env[i] = g_dry_gate_slow_env_coeff * band_slow_env[i]
                           + (1.0f - g_dry_gate_slow_env_coeff) * level;

        const float open_threshold = g_dry_gate_open_threshold[i];
        float onset_threshold = band_slow_env[i] * g_dry_gate_onset_ratio + g_dry_gate_onset_floor;
        onset_threshold *= kDryGateBandOnsetMul[i];
        if(onset_threshold < open_threshold)
        {
            onset_threshold = open_threshold;
        }

        if(band_fast_env[i] >= onset_threshold)
        {
            // One-shot transient window: passa l'attacco, poi richiude il sustain.
            band_hold[i] = g_dry_gate_hold_samples;
        }

        float hold_drive = 0.0f;
        if(band_hold[i] > 0.0f)
        {
            if(g_dry_gate_hold_samples > 1.0f)
            {
                hold_drive = Clamp01(band_hold[i] * g_dry_gate_hold_inv);
            }
            else
            {
                hold_drive = 1.0f;
            }
            band_hold[i] -= 1.0f;
        }

        float transient_drive = 0.0f;
        float drive_span = onset_threshold * kDryGateBandDriveWidth[i] + 1.0e-6f;
        if(band_fast_env[i] > onset_threshold)
        {
            transient_drive = Clamp01((band_fast_env[i] - onset_threshold) / drive_span);
        }

        float drive = (transient_drive > hold_drive) ? transient_drive : hold_drive;
        drive = ShapeDryGateDrive(i, drive);

        float closed_gain = kDryGateBandClosedGain[i];
        float target = closed_gain + (1.0f - closed_gain) * drive;

        float coeff = (target > band_gain[i]) ? g_dry_gate_open_coeff[i]
                                             : g_dry_gate_close_coeff[i];
        band_gain[i] = coeff * band_gain[i] + (1.0f - coeff) * target;
        out += band * band_gain[i];
    }
    return out * g_dry_gate_makeup;
}

inline void RecoverDryGateState(float* band_fast_env,
                                float* band_slow_env,
                                float* band_gain,
                                float* band_hold)
{
    for(size_t i = 0; i < kDryGateBands; ++i)
    {
        band_fast_env[i] *= 0.994f;
        band_slow_env[i] *= 0.997f;
        band_gain[i] += 0.015f * (1.0f - band_gain[i]);
        band_hold[i] = 0.0f;
    }
}
}

// ============================================================================
// PANNELLO DI CONTROLLO DEGLI EFFETTI 
// ============================================================================
// 1 = effetto completamente attivo, 0 = bypassato 

#define ENABLE_LIGHT_EFFECT         1   // filtro Moog controllato dalla luminosità
#define ENABLE_TEMPERATURE_EFFECT   1   // distorsione + comb controllati dalla temperatura
#define ENABLE_FLANGER_EFFECT       1   // flanger temperatura fredda -20→12°C
#define ENABLE_SPECTRAL_GATE_EFFECT 1   // gate umidità 0-40%
#define ENABLE_DELAY_EFFECT         1   // delay stereo umidità 40-100%
#define ENABLE_HUMIDITY_EFFECT      1   // reverb umidità 40-100%

// ---------------------------------------------------------------------------
// FILTRO LUMINOSITÀ - MOOG LADDER
// La luminosita' controlla solo cutoff e resonance del Moog filter.
// Il sensore viene interpretato nel range 0..100.
// ---------------------------------------------------------------------------
static constexpr float kLightSensorMin = 0.0f;
static constexpr float kLightSensorMax = 100.0f;

// Frequenza cutoff minima e massima del filtro Moog controllato dalla luce.
static constexpr float kLightFreqMinHz = 100.0f;
static constexpr float kLightFreqMaxHz = 22000.0f;

// Risonanza minima e massima del filtro Moog controllato dalla luce.
static constexpr float kLightResMin = 0.00f;
static constexpr float kLightResMax = 0.50f;

// Smoothing per evitare cambiamenti bruschi di cutoff e resonance quando la luce varia rapidamente.
static constexpr float kLightFreqSmooth = 0.10f;
static constexpr float kLightResSmooth = 0.02f;

// Finestra di bypass totale del filtro Moog: sopra questa luminosità, il filtro è completamente bypassato per risparmiare CPU e mantenere il suono naturale.
static constexpr float kLightBypassStart = 0.96f;
static constexpr float kLightBypassEnd = 1.00f;

// ---------------------------------------------------------------------------
// TUNING PRINCIPALE FREDDO - COMB FILTER
// Intervenire qui per cambiare i paramentri realtivi al freddo (comb-filter)  
// ---------------------------------------------------------------------------

// kCombStartC / kCombEndC: finestra termica del freddo
static constexpr float kCombStartC = -20.0f;
static constexpr float kCombEndC = 15.0f;      

static constexpr bool kCombReverse = true;
static constexpr bool kCombKeepAfterEnd = false;

// kColdSpacePreDelay*: tempi delle riflessioni iniziali
static constexpr float kColdSpacePreDelayMinMs = 0.5f;
static constexpr float kColdSpacePreDelayMaxMs = 24.0f;

// Offset aggiuntivo per evitare che il pre-delay sia troppo corto a temperature vicine a 12°C
static constexpr float kColdSpaceOffsetMs = 5.0f; 

// kColdSpaceFeedback*: quanta coda resta dopo il primo impatto
static constexpr float kColdSpaceFeedbackMin = 0.20f;  // più presente
static constexpr float kColdSpaceFeedbackMax = 0.55f;  // massimo glaciale

// kColdSpaceMix*: presenza del ramo pre-delay/early reflections nel suono finale
static constexpr float kColdSpaceMixMin = 0.15f;       // sempre percepibile
static constexpr float kColdSpaceMixMax = 0.55f;       // molto forte

// kColdSpaceCrossfeed*: quanto il pre-delay "apre" lo stereo: 0 = nessun crossfeed, 1 = segnale completamente incrociato
static constexpr float kColdSpaceCrossfeed = 0.15f;

// kCombSmooth*: quanto velocemente il comb si adatta ai cambiamenti di temperatura (0.01 = molto veloce, 0.1 = più lento e fluido)
static constexpr float kCombDelaySmooth = 0.05f;
static constexpr float kCombParamSmooth = 0.05f;

// Parametri per il tono freddo frequnza di riferimento per il comb
static constexpr float kColdToneFreqMinHz = 350.0f;
static constexpr float kColdToneFreqMaxHz = 1000.0f;

// Parametri per il tono freddo (distorsione + comb secondario) che si sovrappone al comb principale per aggiungere un carattere metallico/spaziale a temperature molto fredde.
static constexpr float kColdToneMixMin = 0.10f;
static constexpr float kColdToneMixMax = 0.75f;

// Il feedback del tono freddo
static constexpr float kColdToneFeedbackMin = 0.15f;
static constexpr float kColdToneFeedbackMax = 0.75f;

// Offset in campioni per comb filter
static constexpr float kColdToneOffsetSamples = 9.0f;

// Boost aggiuntivo sul freddo: come per il caldo, questa finestra permette
// di spingere di più l'effetto in un intervallo termico specifico.
static constexpr float kColdSpaceExtremeStartC = -20.0f;
static constexpr float kColdSpaceExtremeEndC = -12.0f;

static constexpr float kColdSpaceExtremeMixBoost = 0.20f; // boost glaciale
static constexpr float kColdSpaceExtremeFeedbackBoost = 0.20f; // boost glaciale

// ---------------------------------------------------------------------------
// TUNING SECONDARIO FREDDO - FLANGER 
// Si sovrappone al comb filter per aggiungere il carattere metallico/spaziale
// tipico del freddo estremo. Scompare completamente oltre i -12°C.
// ---------------------------------------------------------------------------
static constexpr float kFlangerStartC     = -20.0f;
static constexpr float kFlangerEndC       =  -12.0f;

static constexpr bool  kFlangerReverse    = true;
static constexpr bool  kFlangerKeepAfterEnd = false; 

// Attenzione a non esagerare con feedback e mix per evitare di sovraccaricare la CPU o creare feedback incontrollati.
static constexpr float kFlangerFbMin      = 0.10f;   
static constexpr float kFlangerFbMax      = 0.25f;   

// La profondità di modulazione del flanger: più alta a temperature molto fredde per un effetto più evidente
static constexpr float kFlangerDepthMin   = 0.05f;   // modulazione leggera
static constexpr float kFlangerDepthMax   = 0.20f;   // modulazione intensa a -20°C (reduced for CPU)

// La frequenza dell'LFO del flanger: più lenta a temperature molto fredde per un effetto più dronico e spaziale, più veloce vicino a 12°C per aggiungere movimento senza appesantire troppo il suono.
static constexpr float kFlangerRateCold   = 0.01f;   // Hz — molto lento, drone a -20°C
static constexpr float kFlangerRateWarm   = 0.15f;   // Hz — più vivace vicino a 12°C

// La lunghezza del delay del flanger: più lunga a temperature molto fredde per un effetto più evidente, più corta vicino a 12°C per ridurre il carico CPU e mantenere il suono più naturale.
static constexpr float kFlangerDelayMinNorm = 0.10f; 
static constexpr float kFlangerDelayMaxNorm = 0.85f;  

// La presenza del flanger nel mix: più alta a temperature molto fredde per un effetto più evidente, più bassa vicino a 12°C per ridurre il carico CPU e mantenere il suono più naturale.
static constexpr float kFlangerMixMin     = 0.10f;   // sempre un tocco percettibile
static constexpr float kFlangerMixMax     = 0.35f;   // ~55% wet effettivo a -20°C

// Clamp reale (sotto il max) per evitare zone estreme inutili e rendere il limite operativo.
static constexpr float kFlangerMixMaxSafe = 0.32f;

// ---------------------------------------------------------------------------
// TUNING PRIMARIO CALDO - DISTORSIONE 
// Intervenire qui per cambiare i paramentri realtivi al caldo (distorsione)
// Ogni effetto ha un proprio intervallo [start, end] e puo' essere invertito.
// reverse=false: 0 -> 1, reverse=true: 1 -> 0
// ---------------------------------------------------------------------------

// SEZIONE EFFETTO DRIVE 
static constexpr float kDriveStartC = 0.0f;
static constexpr float kDriveEndC = 40.0f;

// La quantità di distorsione del drive
static constexpr float kDriveMin = 0.20f;
static constexpr float kDriveMax = 0.80f;

static constexpr bool kDriveReverse = false;
static constexpr bool kDriveKeepAfterEnd = true;

// SEZIONE EFFETTO SOFT CLIP SYMMETRIC (simula saturazione valvolare leggera)
static constexpr float kSerialSoftStartC = 0.0f;
static constexpr float kSerialSoftEndC = 40.0f;

// La quantità di distorsione del soft clip
static constexpr float kSerialSoftMixMin = 0.25f;
static constexpr float kSerialSoftMixMax = 0.50f;

static constexpr bool kSerialSoftReverse = false;
static constexpr bool kSerialSoftKeepAfterEnd = true;

// SEZIONE EFFETTO TUBE (simula saturazione valvolare più spinta)
static constexpr float kSerialTubeStartC = 20.0f;  
static constexpr float kSerialTubeEndC = 40.0f;

// La quantità di distorsione del tube
static constexpr float kSerialTubeMixMin = 0.01f;
static constexpr float kSerialTubeMixMax = 0.15f;

static constexpr bool kSerialTubeReverse = false;
static constexpr bool kSerialTubeKeepAfterEnd = true;

// SEZIONE EFFETTO DIODE CLIP (simula distorsione a diodi, più aggressiva e tagliente)
static constexpr float kSerialDiodeStartC = 20.0f; 
static constexpr float kSerialDiodeEndC = 40.0f;

// La quantità di distorsione del diode clip
static constexpr float kSerialDiodeMixMin = 0.01f;
static constexpr float kSerialDiodeMixMax = 0.02f;

static constexpr bool kSerialDiodeReverse = false;
static constexpr bool kSerialDiodeKeepAfterEnd = true;

// Boost aggiuntivo sul caldo: come per il freddo, questa finestra permette
// di spingere di piu' la distorsione in un intervallo termico specifico.
static constexpr float kHotBoostStartC = 10.0f;
static constexpr float kHotBoostEndC = 40.0f;

static constexpr float kHotDriveBoost = 0.15f;

static constexpr float kHotSerialSoftBoost = 0.25f;

static constexpr float kHotSerialTubeBoost = 0.01f;

static constexpr float kHotSerialDiodeBoost = 0.001f;

// ---------------------------------------------------------------------------
// TUNING DRY MULTIBAND GATE — UMIDITÀ 0..40%
// Divide il segnale in 3 bande SVF e attenua le code deboli banda per banda.
// ---------------------------------------------------------------------------
static constexpr float kDryGateHumidStart    = 0.0f;   // umidità % → gate massimo
static constexpr float kDryGateHumidEnd      = 40.0f;  // umidità % → gate scompare

static constexpr bool  kDryGateReverse       = true;
static constexpr bool  kDryGateKeepAfterEnd  = false;

// Mix per le 3 bande del gate secco
static constexpr float kDryGateApplyMixMin   = 0.20f;
static constexpr float kDryGateApplyMixMax   = 0.70f;

// ---------------------------------------------------------------------------
// TUNING PRIMARIO UMIDITÀ — RIVERBERO
// Intervenire qui per cambiare la risposta del riverbero all'umidità:
// ---------------------------------------------------------------------------
static constexpr float kHumidPlateStart      = 40.0f;   // umidità % → inizio riverbero
static constexpr float kHumidPlateEnd        = 100.0f;  // umidità % → riverbero al massimo

static constexpr bool  kHumidPlateReverse    = false;
static constexpr bool  kHumidPlateKeepAfterEnd = true;

// Feedback del riverbero: più alti a umidità elevate per code più lunghe e più presenza del riverbero.
static constexpr float kPlateRevFbMin        = 0.55f;   // feedback minimo (umidità 40%) — coda breve
static constexpr float kPlateRevFbMax        = 0.90f;   // feedback massimo (umidità 100%) — coda lunga ma non infinita

// Mix del riverbero: più alto a umidità elevate per un effetto più evidente, più basso a umidità vicine a 40% per mantenere il suono più naturale e non sovraccaricare la CPU.
static constexpr float kPlateRevMixMin       = 0.10f;   // wet mix minimo (umidità 40%) → ~4% wet effettivo
static constexpr float kPlateRevMixMax       = 0.50f;   // wet mix massimo (umidità 100%) → ~44% wet effettivo

// Damping del riverbero: più alto a umidità elevate per un suono più scuro e avvolgente
static constexpr float kReverbDampMaxHz      = 12000.0f; // damping a bassa umidità (brillante)
static constexpr float kReverbDampMinHz      = 10000.0f; // damping ad alta umidità (scuro)

// Makeup del riverbero: attenua leggermente il segnale riverberato a umidità elevate per evitare di sovraccaricare la CPU o creare feedback incontrollati, e per mantenere il suono più naturale.
static constexpr float kReverbHumidTrimScale = 0.10f;   // attenuazione makeup al 100% umidità
static constexpr float kReverbHumidTrimMin   = 0.88f;   // trim minimo (clamp di sicurezza)

static constexpr float kReverbFeedbackMaxSafe = 0.88f;  // clamp operativo per tenere stabile la coda alta
static constexpr float kReverbMixMaxSafe      = 0.50f;  // clamp antioscillazione mix

// ---------------------------------------------------------------------------
// TUNING DELAY STEREO — UMIDITÀ 40..100%
// Il delay aggiunge echi ritmici e spazio prima del riverbero.
// La catena è: delay → reverb, quindi gli echi vengono riverberati.
// ---------------------------------------------------------------------------
static constexpr size_t kDelayLineSamples         = 24000u;  // 500ms a 48kHz → 96KB/ch SDRAM

static constexpr float  kDelayHumidStart          = 40.0f;
static constexpr float  kDelayHumidEnd            = 100.0f;

static constexpr bool   kDelayReverse             = false;
static constexpr bool   kDelayKeepAfterEnd        = true;

// Tempo di delay: più lungo a umidità elevate
// Nota anti-artifacts: modulare il delay-time durante audio attivo introduce
// inevitabilmente effetto Doppler/pitch. Usiamo tempo fisso e facciamo variare
// solo feedback/mix con l'umidità.
static constexpr float  kDelayTimeSecMin          = 0.24f;
static constexpr float  kDelayTimeSecMax          = 0.24f;

// Smoothing dedicato al delay-time (block-rate).
static constexpr float kDelayTimeSmoothBlock = 0.20f;

// Feedback del delay
static constexpr float  kDelayFeedbackMin         = 0.15f;   // poche ripetizioni
static constexpr float  kDelayFeedbackMax         = 0.75f;   // più ripetizioni

// Mix del delay
static constexpr float  kDelayMixMin              = 0.10f;   // appena percettibile a 40%
static constexpr float  kDelayMixMax              = 0.50f;   // ~31% wet effettivo a 100%

// Clamp per evitare di sovraccaricare la CPU o creare feedback incontrollati con mix e feedback troppo alti.
static constexpr float  kDelayMixMaxSafe          = 0.42f;   // clamp antioscillazione

// Offset stereo per evitare che i delay di sinistra e destra siano troppo simili, che può causare cancellazioni di fase e un suono più sottile. Questo offset aggiunge una leggera differenza tra i due canali, creando un'immagine stereo più ampia e interessante.
static constexpr size_t kDelayStereoOffsetSamples = 360u;    // 7.5ms spread L↔R

// Buffer delay in SDRAM (96KB per canale, 192KB totali — ~2.4% degli 8MB disponibili)
DSY_SDRAM_BSS static daisysp::DelayLine<float, kDelayLineSamples> g_delay_left;
DSY_SDRAM_BSS static daisysp::DelayLine<float, kDelayLineSamples> g_delay_right;

static float g_effective_reverb_feedback = kPlateRevFbMin;

// ---------------------------------------------------------------------------
// TUNING USCITA
// Intervenire qui per recuperare o trattenere livello quando aumentano gli effetti.
// Compensazione leggera basata sull'intensita' dell'effetto, senza compressore.
// ---------------------------------------------------------------------------
static constexpr float kTempCompMin = 0.65f;      // clamp minimo compensazione gain temperatura

static constexpr float kTempPostTrimMin = 0.35f;  // clamp minimo trim post-catena caldo

static constexpr float kTempLowBoostMax = 0.35f;  // +35% a temperatura bassa
static constexpr float kTempHighCutMax = 0.90f;   // -90% a temperatura alta

static constexpr float kTempLowBoostStartC = -20.0f;
static constexpr float kTempLowBoostEndC = 12.0f;


static constexpr float kTempHighCutStartC = 20.0f;
static constexpr float kTempHighCutEndC = 40.0f;

static constexpr float kHumidityHighBoostStart = 60.0f;
static constexpr float kHumidityHighBoostEnd = 100.0f;

static constexpr float kHumidityHighOutputBoostMax = 0.65f; // +65% vicino a umidità alta

static constexpr float kOutputGainMax = 1.20f;

// Compensazione dedicata a sorgenti esterne (linea senza preamp):
// cresce verso wet pieno per ridurre il calo percepito dry->wet.
// Il microfono interno resta invariato.
static constexpr float kExternalWetMakeupMaxBoost = 0.80f;

// Makeup finale
static constexpr float kOutputMakeupGain = 0.98f;  // leggermente sotto 1 per sicurezza
static constexpr uint32_t kControlUpdateDivider = 48u;

// Precomputed block-rate smoothing coefficients (k_block = 1 - (1 - k_sample)^32).
static constexpr float kLightFreqSmoothBlock = 0.966f;
static constexpr float kLightResSmoothBlock  = 0.476f;
static constexpr float kCombDelaySmoothBlock = 0.806f;
static constexpr float kCombParamSmoothBlock = 0.806f;

// ============================================================================
// INIZIALIZZAZIONE
// ============================================================================

void AudioProcessor::Init(float sample_rate)
{
    sample_rate_ = sample_rate;
    input_dc_block_coeff_ = expf(-2.0f * 3.14159265358979323846f
                                 * 20.0f / sample_rate_);
    input_prev_l_ = 0.0f;
    input_prev_r_ = 0.0f;
    input_dc_state_l_ = 0.0f;
    input_dc_state_r_ = 0.0f;

    // ========================================================================
    // INIZIALIZZAZIONE FILTRI E DISTORSIONE
    // ========================================================================
    current_filter_freq_ = 1400.0f;
    current_filter_res_ = 0.12f;

    filter_left_.Init(sample_rate_);
    filter_right_.Init(sample_rate_);
    filter_left_.SetInputDrive(1.0f);
    filter_right_.SetInputDrive(1.0f);
    filter_left_.SetFreq(current_filter_freq_);
    filter_right_.SetFreq(current_filter_freq_);
    filter_left_.SetRes(current_filter_res_);
    filter_right_.SetRes(current_filter_res_);

    overdrive_.Init();
    overdrive_.SetDrive(0.0f);

    comb_left_.Init();
    comb_right_.Init();
    comb_left_.SetDelay(84.0f);
    comb_right_.SetDelay(91.0f);

    cold_tone_left_.Init();
    cold_tone_right_.Init();
    cold_tone_left_.SetDelay(84.0f);
    cold_tone_right_.SetDelay(89.0f);

    for(size_t i = 0; i < kCombDelayMaxSamples; i++)
    {
        comb_left_.Write(0.0f);
        comb_right_.Write(0.0f);
        cold_tone_left_.Write(0.0f);
        cold_tone_right_.Write(0.0f);
    }

    // ========================================================================
    // INIZIALIZZAZIONE REVERB (ReverbSc — SRAM interna, niente SDRAM)
    // ========================================================================
    reverb_ptr_ = &g_reverb;
    reverb_ptr_->Init(sample_rate_);
    reverb_ptr_->SetFeedback(kPlateRevFbMin);
    reverb_ptr_->SetLpFreq(kReverbDampMaxHz);

    // ========================================================================
    // INIZIALIZZAZIONE FLANGER STEREO (due istanze mono — ~8KB SRAM interna)
    // ========================================================================
    flanger_left_.Init(sample_rate_);
    flanger_right_.Init(sample_rate_);
    flanger_left_.SetDelay(kFlangerDelayMaxNorm);
    flanger_right_.SetDelay(kFlangerDelayMaxNorm);
    flanger_left_.SetLfoFreq(kFlangerRateWarm);
    flanger_right_.SetLfoFreq(kFlangerRateWarm);
    flanger_left_.SetLfoDepth(kFlangerDepthMin);
    flanger_right_.SetLfoDepth(kFlangerDepthMin);
    flanger_left_.SetFeedback(kFlangerFbMin);
    flanger_right_.SetFeedback(kFlangerFbMin);

    // ========================================================================
    // INIZIALIZZAZIONE DRY MULTIBAND GATE
    // ========================================================================
    for(size_t i = 0; i < kDryGateBands; ++i)
    {
        g_dry_gate_filters_l[i].Init(sample_rate_);
        g_dry_gate_filters_r[i].Init(sample_rate_);
        g_dry_gate_filters_l[i].SetFreq(kDryGateBandFreqs[i]);
        g_dry_gate_filters_r[i].SetFreq(kDryGateBandFreqs[i]);
        g_dry_gate_filters_l[i].SetRes(0.58f);
        g_dry_gate_filters_r[i].SetRes(0.58f);
        g_dry_gate_env_l[i] = 0.0f;
        g_dry_gate_env_r[i] = 0.0f;
        g_dry_gate_slow_env_l[i] = 0.0f;
        g_dry_gate_slow_env_r[i] = 0.0f;
        g_dry_gate_gain_l[i] = 1.0f;
        g_dry_gate_gain_r[i] = 1.0f;
        g_dry_gate_hold_l[i] = 0.0f;
        g_dry_gate_hold_r[i] = 0.0f;
    }
    UpdateDryGateRuntime(sample_rate_, 0.0f);

    // ========================================================================
    // INIZIALIZZAZIONE DELAY STEREO (buffer in SDRAM — 96KB/ch)
    // ========================================================================
    g_delay_left.Init();
    g_delay_right.Init();
    const size_t delay_init_samples = (size_t)(sample_rate_ * kDelayTimeSecMin);
    g_delay_left.SetDelay(delay_init_samples);
    g_delay_right.SetDelay(delay_init_samples + kDelayStereoOffsetSamples);

    current_temp_shape_       = 0.0f;
    current_overdrive_drive_  = 0.0f;
    current_humidity_shape_   = 0.0f;
    current_reverb_feedback_  = kPlateRevFbMin;
    current_reverb_mix_       = 0.0f;
    current_reverb_damp_      = kReverbDampMaxHz;
    current_temp_gain_comp_   = 1.0f;
    current_temp_shape_curve_ = 0.0f;
    current_comb_mix_         = 0.0f;
    current_comb_delay_       = sample_rate_ * (kColdSpacePreDelayMinMs * 0.001f);
    current_comb_feedback_    = 0.0f;
    current_cold_tone_mix_    = 0.0f;
    current_cold_tone_delay_  = 84.0f;
    current_cold_tone_feedback_ = 0.0f;
    current_delay_time_       = (float)delay_init_samples;
    current_delay_feedback_   = kDelayFeedbackMin;
    current_delay_mix_        = 0.0f;
    current_flanger_feedback_ = kFlangerFbMin;
    current_flanger_rate_     = kFlangerRateWarm;
    current_flanger_depth_    = kFlangerDepthMin;
    current_flanger_mix_      = 0.0f;
    cpu_relief_               = 0.0f;
    control_update_counter_   = 0u;
    light_bypass_             = 0.0f;
    dry_gate_amount_          = 0.0f;
    effect_output_gain_       = 1.0f;

    target_filter_freq_          = current_filter_freq_;
    target_filter_res_           = current_filter_res_;
    target_temp_shape_           = 0.0f;
    target_comb_mix_             = 0.0f;
    target_comb_delay_           = current_comb_delay_;
    target_comb_feedback_        = current_comb_feedback_;
    target_cold_tone_mix_        = 0.0f;
    target_cold_tone_delay_      = current_cold_tone_delay_;
    target_cold_tone_feedback_   = current_cold_tone_feedback_;
    target_overdrive_drive_      = 0.0f;
    target_serial_soft_mix_      = 0.0f;
    target_serial_tube_mix_      = 0.0f;
    target_serial_diode_mix_     = 0.0f;
    target_humidity_shape_       = 0.0f;
    target_reverb_feedback_      = current_reverb_feedback_;
    target_reverb_mix_           = current_reverb_mix_;
    target_delay_time_           = current_delay_time_;
    target_delay_feedback_       = current_delay_feedback_;
    target_delay_mix_            = 0.0f;
    target_flanger_feedback_     = kFlangerFbMin;
    target_flanger_rate_         = kFlangerRateWarm;
    target_flanger_depth_        = kFlangerDepthMin;
    target_flanger_mix_          = 0.0f;
}

// ============================================================================
// PROCESSAMENTO AUDIO
// ============================================================================

void AudioProcessor::Process(
    float in_left, 
    float in_right,
    float light,
    float temperature,
    float humidity,
    float encoder_mix,
    float volume,
    bool external_input,
    bool rec_active,
    float& out_left,
    float& out_right
)
{
    (void)rec_active;

    // Gentle input DC blocker (~20 Hz). This attenuates supply/ground steps
    // coupled from peripheral activity without touching the useful spectrum.
    const float dc_blocked_l = in_left - input_prev_l_
                               + input_dc_block_coeff_ * input_dc_state_l_;
    const float dc_blocked_r = in_right - input_prev_r_
                               + input_dc_block_coeff_ * input_dc_state_r_;
    input_prev_l_ = in_left;
    input_prev_r_ = in_right;
    input_dc_state_l_ = dc_blocked_l;
    input_dc_state_r_ = dc_blocked_r;
    in_left = dc_blocked_l;
    in_right = dc_blocked_r;

    const float wet_mix_amount = Clamp01(encoder_mix);

    // Full bypass path when user sets dry/wet to 0.
    // Keep only master/output gain behavior active.
    if(wet_mix_amount <= 0.0f)
    {
        float out_gain = volume * kOutputMakeupGain;
        if(out_gain > kOutputGainMax)
            out_gain = kOutputGainMax;

        out_left  = SanitizeFinite(in_left * out_gain);
        out_right = SanitizeFinite(in_right * out_gain);

        out_left  = HardClip(out_left, 0.995f);
        out_right = HardClip(out_right, 0.995f);
        return;
    }

    const float wet_ctrl = SmoothStep(0.0f, 1.0f, wet_mix_amount);
    const float humidity_effect_percent = current_humidity_shape_ * 100.0f;

    // Adaptive load control: if wet is high and parameters move quickly,
    // progressively lighten the heaviest branches to avoid callback starvation.
    float param_motion = fabsf(target_temp_shape_ - current_temp_shape_)
                         + fabsf(target_humidity_shape_ - current_humidity_shape_)
                         + fabsf(target_delay_mix_ - current_delay_mix_)
                         + fabsf(target_reverb_mix_ - current_reverb_mix_)
                         + fabsf(target_flanger_mix_ - current_flanger_mix_);
    float heavy_overlap = current_temp_shape_ * (current_delay_mix_ + current_reverb_mix_);
    float relief_drive = SmoothStep(0.70f, 0.98f, wet_ctrl)
                         * SmoothStep(0.12f, 0.60f, param_motion + heavy_overlap);
    cpu_relief_ += 0.08f * (relief_drive - cpu_relief_);
    if(cpu_relief_ < 0.0f)
        cpu_relief_ = 0.0f;
    if(cpu_relief_ > 1.0f)
        cpu_relief_ = 1.0f;

    // ========================================================================
    // STEP 1: CONVERTI SENSORI IN PARAMETRI EFFETTI
    // ========================================================================
    
    const bool refresh_control_targets = (control_update_counter_ == 0u);
    if(refresh_control_targets)
    {
        control_update_counter_ = kControlUpdateDivider - 1u;
        const float humidity_percent = SensorToPercent(humidity);

    // Luminosita' -> filtro Moog principale
#if ENABLE_LIGHT_EFFECT
        target_filter_freq_ = LightToFilterFreq(light);
        target_filter_res_ = LightToFilterRes(light);
        const float light_percent = (light <= 1.0f) ? (light * 100.0f) : light;
        const float light_amount = Clamp01((light_percent - kLightSensorMin)
                                          / (kLightSensorMax - kLightSensorMin));
        light_bypass_ = SmoothStep(kLightBypassStart, kLightBypassEnd, light_amount);
#else
        target_filter_freq_ = current_filter_freq_;
        target_filter_res_ = current_filter_res_;
#endif

    // Temperatura -> catena timbrica graduale
#if ENABLE_TEMPERATURE_EFFECT
        target_temp_shape_ = TemperatureToShape(temperature);

        float comb_amount = RangeAmount(temperature,
                                        kCombStartC,
                                        kCombEndC,
                                        kCombReverse,
                                        kCombKeepAfterEnd);
        float comb_curve = SmoothStep(0.0f, 1.0f, comb_amount);
        float extreme_cold_amount = RangeAmount(temperature,
                                                kColdSpaceExtremeStartC,
                                                kColdSpaceExtremeEndC,
                                                true,
                                                true);
        target_comb_mix_ = StageValue(comb_curve, kColdSpaceMixMin, kColdSpaceMixMax)
                           + kColdSpaceExtremeMixBoost * extreme_cold_amount;
        target_comb_delay_ = sample_rate_
                             * ((kColdSpacePreDelayMinMs
                                 + (kColdSpacePreDelayMaxMs - kColdSpacePreDelayMinMs)
                                       * comb_curve)
                                * 0.001f);
        target_comb_feedback_ = StageValue(comb_curve,
                                           kColdSpaceFeedbackMin,
                                           kColdSpaceFeedbackMax)
                                + kColdSpaceExtremeFeedbackBoost * extreme_cold_amount;
        target_cold_tone_mix_ = StageValue(comb_curve,
                                           kColdToneMixMin,
                                           kColdToneMixMax);
        float target_cold_tone_freq = kColdToneFreqMinHz
                                      + (kColdToneFreqMaxHz - kColdToneFreqMinHz) * comb_curve;
        target_cold_tone_delay_ = sample_rate_ / target_cold_tone_freq;
        target_cold_tone_feedback_ = StageValue(comb_curve,
                                                kColdToneFeedbackMin,
                                                kColdToneFeedbackMax);

#if ENABLE_FLANGER_EFFECT
        // Flanger: stessa curva del comb (-20→12°C, reverse)
        float flanger_amount = RangeAmount(temperature,
                                            kFlangerStartC,
                                            kFlangerEndC,
                                            kFlangerReverse,
                                            kFlangerKeepAfterEnd);
        target_flanger_feedback_ = StageValue(flanger_amount, kFlangerFbMin, kFlangerFbMax);
        target_flanger_depth_    = StageValue(flanger_amount, kFlangerDepthMin, kFlangerDepthMax);
        target_flanger_mix_      = StageValue(flanger_amount, kFlangerMixMin, kFlangerMixMax);
        // Delay interpolato: 1 (freddo) → min, 0 (caldo) → max
        float flanger_delay_norm = StageValue(flanger_amount, kFlangerDelayMinNorm, kFlangerDelayMaxNorm);
        // Rate inverso: flanger_amount=1 → kFlangerRateCold (lento), 0 → kFlangerRateWarm
        target_flanger_rate_ = kFlangerRateWarm
                       + (kFlangerRateCold - kFlangerRateWarm) * flanger_amount;
        if(target_flanger_mix_ > kFlangerMixMaxSafe) target_flanger_mix_ = kFlangerMixMaxSafe;
        // Aggiorna subito il delay (non serve smoothing, è lento)
        flanger_left_.SetDelay(flanger_delay_norm);
        flanger_right_.SetDelay(flanger_delay_norm);
#endif

        float drive_amount = RangeAmount(temperature,
                                         kDriveStartC,
                                         kDriveEndC,
                                         kDriveReverse,
                                         kDriveKeepAfterEnd);
        float drive_curve = drive_amount * drive_amount * (0.55f + 0.45f * drive_amount);

        float hot_boost_amount = RangeAmount(temperature,
                                             kHotBoostStartC,
                                             kHotBoostEndC,
                                             false,
                                             true);

        target_overdrive_drive_ = StageValue(drive_amount, kDriveMin, kDriveMax)
                                  * (0.78f + 0.22f * drive_curve)
                                  + kHotDriveBoost * hot_boost_amount;

        float serial_soft_amount = RangeAmount(temperature,
                                               kSerialSoftStartC,
                                               kSerialSoftEndC,
                                               kSerialSoftReverse,
                                               kSerialSoftKeepAfterEnd);
        float serial_tube_amount = RangeAmount(temperature,
                                               kSerialTubeStartC,
                                               kSerialTubeEndC,
                                               kSerialTubeReverse,
                                               kSerialTubeKeepAfterEnd);
        float serial_diode_amount = RangeAmount(temperature,
                                                kSerialDiodeStartC,
                                                kSerialDiodeEndC,
                                                kSerialDiodeReverse,
                                                kSerialDiodeKeepAfterEnd);

        target_serial_soft_mix_ = StageValue(serial_soft_amount,
                                             kSerialSoftMixMin,
                                             kSerialSoftMixMax)
                                   + kHotSerialSoftBoost * hot_boost_amount;
        target_serial_tube_mix_ = StageValue(serial_tube_amount,
                                             kSerialTubeMixMin,
                                             kSerialTubeMixMax)
                                   + kHotSerialTubeBoost * hot_boost_amount;
        target_serial_diode_mix_ = StageValue(serial_diode_amount,
                                              kSerialDiodeMixMin,
                                              kSerialDiodeMixMax)
                                    + kHotSerialDiodeBoost * hot_boost_amount;
        if(target_serial_soft_mix_ > 1.0f)
        {
            target_serial_soft_mix_ = 1.0f;
        }
        if(target_serial_tube_mix_ > 1.0f)
        {
            target_serial_tube_mix_ = 1.0f;
        }
        if(target_serial_diode_mix_ > 1.0f)
        {
            target_serial_diode_mix_ = 1.0f;
        }
#else
        target_temp_shape_ = 0.0f;
        target_comb_mix_ = 0.0f;
        target_comb_delay_ = current_comb_delay_;
        target_comb_feedback_ = current_comb_feedback_;
        target_cold_tone_mix_ = 0.0f;
        target_cold_tone_delay_ = current_cold_tone_delay_;
        target_cold_tone_feedback_ = current_cold_tone_feedback_;
        target_overdrive_drive_ = 0.0f;
        target_serial_soft_mix_ = 0.0f;
        target_serial_tube_mix_ = 0.0f;
        target_serial_diode_mix_ = 0.0f;
        target_flanger_feedback_ = kFlangerFbMin;
        target_flanger_rate_     = kFlangerRateWarm;
        target_flanger_depth_    = kFlangerDepthMin;
        target_flanger_mix_      = 0.0f;
#endif

#if ENABLE_HUMIDITY_EFFECT
        target_humidity_shape_ = HumidityToShape(humidity_percent);
        float plate_amount = RangeAmount(humidity_effect_percent,
                                        kHumidPlateStart,
                                        kHumidPlateEnd,
                                        kHumidPlateReverse,
                                        kHumidPlateKeepAfterEnd);
        target_reverb_feedback_ = StageValue(plate_amount, kPlateRevFbMin, kPlateRevFbMax);
        target_reverb_mix_      = StageValue(plate_amount, kPlateRevMixMin, kPlateRevMixMax);
        // Damping: più umidità → suono più scuro (frequenza LP più bassa)
        float target_reverb_damp = kReverbDampMaxHz
                                   - (kReverbDampMaxHz - kReverbDampMinHz) * plate_amount;
        if(target_reverb_feedback_ > kReverbFeedbackMaxSafe)
            target_reverb_feedback_ = kReverbFeedbackMaxSafe;
        if(target_reverb_feedback_ < 0.0f)
            target_reverb_feedback_ = 0.0f;
        if(target_reverb_mix_ > kReverbMixMaxSafe)
            target_reverb_mix_ = kReverbMixMaxSafe;
        if(target_reverb_mix_ < 0.0f)
            target_reverb_mix_ = 0.0f;
        current_reverb_damp_ += 0.08f * (target_reverb_damp - current_reverb_damp_);
#else
        target_humidity_shape_ = 0.0f;
        target_reverb_feedback_ = kPlateRevFbMin;
        target_reverb_mix_ = 0.0f;
#endif

#if ENABLE_SPECTRAL_GATE_EFFECT
    float dry_gate_control_amount = RangeAmount(humidity_percent,
                            kDryGateHumidStart,
                            kDryGateHumidEnd,
                            kDryGateReverse,
                            kDryGateKeepAfterEnd);
    UpdateDryGateRuntime(sample_rate_, dry_gate_control_amount);
    dry_gate_amount_ = dry_gate_control_amount;
#endif

#if ENABLE_DELAY_EFFECT
        // Delay: cresce da 40% a 100% di umidità (parallelo al reverb)
        float delay_amount = RangeAmount(humidity_percent,
                                          kDelayHumidStart,
                                          kDelayHumidEnd,
                                          kDelayReverse,
                                          kDelayKeepAfterEnd);
        target_delay_time_ = StageValue(delay_amount,
                                         sample_rate_ * kDelayTimeSecMin,
                                         sample_rate_ * kDelayTimeSecMax);
        if(target_delay_time_ > (float)(kDelayLineSamples - 1u))
            target_delay_time_ = (float)(kDelayLineSamples - 1u);
        target_delay_feedback_ = StageValue(delay_amount, kDelayFeedbackMin, kDelayFeedbackMax);
        target_delay_mix_      = StageValue(delay_amount, kDelayMixMin, kDelayMixMax);
        if(target_delay_mix_ > kDelayMixMaxSafe) target_delay_mix_ = kDelayMixMaxSafe;
#else
        target_delay_time_     = current_delay_time_;
        target_delay_feedback_ = kDelayFeedbackMin;
        target_delay_mix_      = 0.0f;
#endif

        // ====================================================================
        // STEP 2: SMOOTHING + AGGIORNAMENTO COEFFICIENTI (block-rate, 1/32)
        // ====================================================================
        // Coefficienti scalati per block-rate (ogni 32 campioni ≈ 1500 Hz):
        // k_block = 1 - (1 - k_sample)^32
        // Esempi: 0.10 → 0.966 | 0.02 → 0.499 | 0.008 → 0.228 | 0.015 → 0.393
        // Il comportamento percepito è identico: le transizioni rimangono dolci.
        current_filter_freq_ += kLightFreqSmoothBlock * (target_filter_freq_ - current_filter_freq_);
        current_filter_res_  += kLightResSmoothBlock  * (target_filter_res_  - current_filter_res_);
        current_comb_mix_    += kCombParamSmoothBlock * (target_comb_mix_    - current_comb_mix_);
        current_comb_delay_  += kCombDelaySmoothBlock * (target_comb_delay_  - current_comb_delay_);
        current_comb_feedback_ += kCombParamSmoothBlock * (target_comb_feedback_ - current_comb_feedback_);
        current_cold_tone_mix_  += 0.802f * (target_cold_tone_mix_  - current_cold_tone_mix_);
        current_cold_tone_delay_+= 0.802f * (target_cold_tone_delay_ - current_cold_tone_delay_);
        current_cold_tone_feedback_ += 0.802f * (target_cold_tone_feedback_ - current_cold_tone_feedback_);
        current_temp_shape_   += 0.228f * (target_temp_shape_  - current_temp_shape_);
        current_temp_shape_curve_ = current_temp_shape_ * current_temp_shape_;
        float target_temp_gain_comp = 1.0f - 0.26f * current_temp_shape_ - 0.32f * current_temp_shape_curve_;
        if(target_temp_gain_comp < kTempCompMin)
            target_temp_gain_comp = kTempCompMin;
        current_temp_gain_comp_    += 0.274f * (target_temp_gain_comp    - current_temp_gain_comp_);
        current_overdrive_drive_   += 0.228f * (target_overdrive_drive_  - current_overdrive_drive_);
        current_humidity_shape_    += 0.499f * (target_humidity_shape_   - current_humidity_shape_);
        // These mappings depend on controls, not on the current audio sample.
        const float temp_low = RangeAmount(temperature, kTempLowBoostStartC,
                                          kTempLowBoostEndC, true, false);
        const float temp_high = RangeAmount(temperature, kTempHighCutStartC,
                                           kTempHighCutEndC, false, true);
        const float humid_high = RangeAmount(current_humidity_shape_ * 100.0f,
                                             kHumidityHighBoostStart,
                                             kHumidityHighBoostEnd, false, true);
        effect_output_gain_ = (1.0f + kTempLowBoostMax * temp_low)
                              * (1.0f - kTempHighCutMax * temp_high)
                              * (1.0f + kHumidityHighOutputBoostMax * humid_high);
        current_reverb_feedback_   += 0.08f * (target_reverb_feedback_  - current_reverb_feedback_);
        current_reverb_mix_        += 0.08f * (target_reverb_mix_       - current_reverb_mix_);
        // Delay-time smoothing dedicato per ridurre artefatti su salti rapidi di umidità.
        current_delay_time_        += kDelayTimeSmoothBlock * (target_delay_time_ - current_delay_time_);
        current_delay_feedback_    += 0.10f * (target_delay_feedback_   - current_delay_feedback_);
        current_delay_mix_         += 0.10f * (target_delay_mix_        - current_delay_mix_);
        // Flanger: rate con smooth lento (LFO freq cambia percettibilmente se troppo brusco)
        current_flanger_feedback_  += 0.499f * (target_flanger_feedback_ - current_flanger_feedback_);
        current_flanger_rate_      += 0.228f * (target_flanger_rate_     - current_flanger_rate_);
        current_flanger_depth_     += 0.499f * (target_flanger_depth_    - current_flanger_depth_);
        current_flanger_mix_       += 0.499f * (target_flanger_mix_      - current_flanger_mix_);

        // Aggiorna coefficienti effetti (ricalcolo pesante solo a 1500 Hz)
        filter_left_.SetFreq(current_filter_freq_);
        filter_right_.SetFreq(current_filter_freq_);
        filter_left_.SetRes(current_filter_res_);
        filter_right_.SetRes(current_filter_res_);
        overdrive_.SetDrive(current_overdrive_drive_);
        reverb_ptr_->SetLpFreq(current_reverb_damp_);
        flanger_left_.SetFeedback(current_flanger_feedback_);
        flanger_right_.SetFeedback(current_flanger_feedback_);
        flanger_left_.SetLfoFreq(current_flanger_rate_);
        flanger_right_.SetLfoFreq(current_flanger_rate_);
        flanger_left_.SetLfoDepth(current_flanger_depth_);
        flanger_right_.SetLfoDepth(current_flanger_depth_);

        // Aggiorna delay line (comb temperatura)
        comb_left_.SetDelay(current_comb_delay_);
        comb_right_.SetDelay(current_comb_delay_ + sample_rate_ * (kColdSpaceOffsetMs * 0.001f));
        cold_tone_left_.SetDelay(current_cold_tone_delay_);
        cold_tone_right_.SetDelay(current_cold_tone_delay_ + kColdToneOffsetSamples);
        
        // Delay-time fisso per evitare Doppler/pitch-shift durante variazioni rapide.
        g_delay_left.SetDelay((size_t)current_delay_time_);
        g_delay_right.SetDelay((size_t)current_delay_time_ + kDelayStereoOffsetSamples);

        // Smoothing extra sul feedback reverb per evitare scatti udibili.
        g_effective_reverb_feedback += 0.08f * (current_reverb_feedback_ - g_effective_reverb_feedback);
        reverb_ptr_->SetFeedback(g_effective_reverb_feedback);
    }
    else
    {
        --control_update_counter_;
    }

    // ========================================================================
    // STEP 3: APPLICA EFFETTI
    // ========================================================================
    
    // Segnale secco (original) per il dry/wet mix
    float dry_left = in_left;
    float dry_right = in_right;
    
    // APPLICA FILTRO LUCE (Moog ladder)
#if ENABLE_LIGHT_EFFECT
    float filtered_left = filter_left_.Process(in_left);
    float filtered_right = filter_right_.Process(in_right);
    filtered_left = FastLinearMix(filtered_left, in_left, light_bypass_);
    filtered_right = FastLinearMix(filtered_right, in_right, light_bypass_);
#else
    float filtered_left = in_left;
    float filtered_right = in_right;
#endif
    
    float wet_left = filtered_left;
    float wet_right = filtered_right;

#if ENABLE_TEMPERATURE_EFFECT
    const bool temp_section_active = current_comb_mix_ > 0.001f
                                  || current_cold_tone_mix_ > 0.001f
                                  || current_overdrive_drive_ > 0.001f
                                  || current_temp_shape_ > 0.001f
                                  || current_flanger_mix_ > 0.001f;
    const float flanger_mix_eff = current_flanger_mix_ * (1.0f - 0.92f * cpu_relief_);
    const float serial_relief = 1.0f - 0.88f * cpu_relief_;

    if(temp_section_active)
    {
        float cold_reflect_left = comb_left_.Read();
        float cold_reflect_right = comb_right_.Read();
        float cold_write_left = filtered_left + cold_reflect_right * current_comb_feedback_;
        float cold_write_right = filtered_right + cold_reflect_left * current_comb_feedback_;
        comb_left_.Write(cold_write_left);
        comb_right_.Write(cold_write_right);

        // Qui si regola il carattere del pre-delay freddo: più mix/offset = più spazio,
        // più feedback = più riflessioni ripetute. Il crossfeed tiene il ramo stereo coeso.
        float spread_left = cold_reflect_left * (1.0f - kColdSpaceCrossfeed)
                    + cold_reflect_right * kColdSpaceCrossfeed;
        float spread_right = cold_reflect_right * (1.0f - kColdSpaceCrossfeed)
                     + cold_reflect_left * kColdSpaceCrossfeed;
        float cold_trim = 1.0f - 0.08f * current_comb_mix_;
        wet_left = FastLinearMix(filtered_left, filtered_left + spread_left, current_comb_mix_)
               * cold_trim;
        wet_right = FastLinearMix(filtered_right, filtered_right + spread_right, current_comb_mix_)
                * cold_trim;

        // Qui si regola il comb tonale freddo: frequenza, feedback e mix definiscono
        // quanto il freddo diventa metallico e riconoscibile anche a temperature intermedie.
        float cold_tone_left = cold_tone_left_.Read();
        float cold_tone_right = cold_tone_right_.Read();
        cold_tone_left_.Write(wet_left + cold_tone_left * current_cold_tone_feedback_);
        cold_tone_right_.Write(wet_right + cold_tone_right * current_cold_tone_feedback_);
        float cold_tone_trim = 1.0f - 0.06f * current_cold_tone_mix_;
        wet_left = FastLinearMix(wet_left, cold_tone_left, current_cold_tone_mix_)
               * cold_tone_trim;
        wet_right = FastLinearMix(wet_right, cold_tone_right, current_cold_tone_mix_)
                * cold_tone_trim;

        // Flanger freddo — dopo i comb (spaziale + tonale), prima della distorsione
        // DaisySP Flanger::Process() restituisce 50% dry + 50% wet internamente.
        // Estraiamo il solo wet con: fl_wet = 2*Process(x) - x
#if ENABLE_FLANGER_EFFECT
        if(flanger_mix_eff > 0.02f)
        {
            float raw_left  = flanger_left_.Process(wet_left);
            float raw_right = flanger_right_.Process(wet_right);
            float fl_wet_left  = 2.0f * raw_left  - wet_left;
            float fl_wet_right = 2.0f * raw_right - wet_right;
            wet_left  = FastLinearMix(wet_left,  fl_wet_left,  flanger_mix_eff);
            wet_right = FastLinearMix(wet_right, fl_wet_right, flanger_mix_eff);
        }
#endif

        float temp_ref_energy_left = wet_left;
        float temp_ref_energy_right = wet_right;

        // Below the hot region this entire result is mixed out at exactly zero.
        if(current_temp_shape_curve_ > 0.0f)
        {
            float overdriven_left = overdrive_.Process(wet_left);
            float overdriven_right = overdrive_.Process(wet_right);
            float harmonic_blend = 0.10f + 0.16f * current_temp_shape_;
              float shaped_left = FastLinearMix(overdriven_left,
                         TubeSaturation(overdriven_left,
                                     1.00f + 0.95f * current_temp_shape_),
                              harmonic_blend);
              float shaped_right = FastLinearMix(overdriven_right,
                          TubeSaturation(overdriven_right,
                                      1.00f + 0.95f * current_temp_shape_),
                               harmonic_blend);
             wet_left = shaped_left;
             wet_right = shaped_right;

            const float current_temp_curve = current_temp_shape_curve_;
            float temp_drive = 1.0f + 0.28f * current_temp_curve;
            wet_left = SoftClipSymmetric(wet_left * temp_drive);
            wet_right = SoftClipSymmetric(wet_right * temp_drive);

            float wet_trim = 0.94f - 0.18f * current_temp_curve;
            if(wet_trim < 0.72f)
            {
                wet_trim = 0.72f;
            }

            wet_left *= wet_trim;
            wet_right *= wet_trim;

            wet_left *= current_temp_gain_comp_;
            wet_right *= current_temp_gain_comp_;

            const float soft_mix_eff = target_serial_soft_mix_ * serial_relief;
            const float tube_mix_eff = target_serial_tube_mix_ * serial_relief;
            const float diode_mix_eff = target_serial_diode_mix_ * serial_relief;
            if((soft_mix_eff + tube_mix_eff + diode_mix_eff) > 0.002f)
            {
                float soft_l = SoftClipSymmetric((wet_left + 0.01f)
                                                 * (1.18f + 0.72f * current_temp_shape_))
                               - 0.01f;
                float soft_r = SoftClipSymmetric((wet_right + 0.01f)
                                                 * (1.18f + 0.72f * current_temp_shape_))
                               - 0.01f;
                wet_left = FastLinearMix(wet_left, soft_l, soft_mix_eff);
                wet_right = FastLinearMix(wet_right, soft_r, soft_mix_eff);

                float tube_l = TubeSaturation(wet_left, 1.0f + 1.5f * current_temp_shape_);
                float tube_r = TubeSaturation(wet_right, 1.0f + 1.5f * current_temp_shape_);
                wet_left = FastLinearMix(wet_left, tube_l, tube_mix_eff);
                wet_right = FastLinearMix(wet_right, tube_r, tube_mix_eff);

                float diode_l = DiodeClip(wet_left * (1.0f + 0.65f * current_temp_shape_),
                                          0.92f - 0.15f * current_temp_shape_);
                float diode_r = DiodeClip(wet_right * (1.0f + 0.65f * current_temp_shape_),
                                          0.92f - 0.15f * current_temp_shape_);
                wet_left = FastLinearMix(wet_left, diode_l, diode_mix_eff);
                wet_right = FastLinearMix(wet_right, diode_r, diode_mix_eff);
            }

            wet_left = FastLinearMix(temp_ref_energy_left, wet_left, current_temp_curve);
            wet_right = FastLinearMix(temp_ref_energy_right, wet_right, current_temp_curve);
        }

        float temp_post_trim = 1.0f - 0.18f * current_temp_shape_ - 0.26f * current_temp_shape_curve_;
        if(temp_post_trim < kTempPostTrimMin)
        {
            temp_post_trim = kTempPostTrimMin;
        }
        wet_left *= temp_post_trim;
        wet_right *= temp_post_trim;
    }
#endif

#if ENABLE_SPECTRAL_GATE_EFFECT
    // Gate sustain-killer in zona secca (0..35% umidità):
    // posizionato post-temperatura per tagliare anche le code generate dalla catena timbrica.
    const float dry_gate_amount = dry_gate_amount_;
    if(dry_gate_amount > 0.002f)
    {
        float gate_apply_mix = StageValue(dry_gate_amount,
                                          kDryGateApplyMixMin,
                                          kDryGateApplyMixMax);
        // When humid delay is active, progressively reduce dry-gate intensity
        // to avoid stacking two expensive time-domain sections in the callback.
        float gate_overlap_scale = 1.0f - 0.85f * current_delay_mix_;
        if(gate_overlap_scale < 0.0f)
            gate_overlap_scale = 0.0f;
        gate_apply_mix *= gate_overlap_scale;

        if(gate_apply_mix > 0.002f)
        {
            float gated_left = ProcessDryMultibandGate(wet_left,
                                                       g_dry_gate_filters_l,
                                                       g_dry_gate_env_l,
                                                       g_dry_gate_slow_env_l,
                                                       g_dry_gate_gain_l,
                                                       g_dry_gate_hold_l);
            float gated_right = ProcessDryMultibandGate(wet_right,
                                                        g_dry_gate_filters_r,
                                                        g_dry_gate_env_r,
                                                        g_dry_gate_slow_env_r,
                                                        g_dry_gate_gain_r,
                                                        g_dry_gate_hold_r);
            wet_left = FastLinearMix(wet_left, gated_left, gate_apply_mix);
            wet_right = FastLinearMix(wet_right, gated_right, gate_apply_mix);
        }
        else
        {
            RecoverDryGateState(g_dry_gate_env_l,
                                g_dry_gate_slow_env_l,
                                g_dry_gate_gain_l,
                                g_dry_gate_hold_l);
            RecoverDryGateState(g_dry_gate_env_r,
                                g_dry_gate_slow_env_r,
                                g_dry_gate_gain_r,
                                g_dry_gate_hold_r);
        }
    }
    else
    {
        RecoverDryGateState(g_dry_gate_env_l,
                            g_dry_gate_slow_env_l,
                            g_dry_gate_gain_l,
                            g_dry_gate_hold_l);
        RecoverDryGateState(g_dry_gate_env_r,
                            g_dry_gate_slow_env_r,
                            g_dry_gate_gain_r,
                            g_dry_gate_hold_r);
    }
#endif

    // APPLICA DELAY (umidità 40-100%) — prima del reverb: gli echi vengono riverberati
#if ENABLE_DELAY_EFFECT
    const float delay_mix_eff = current_delay_mix_ * (1.0f - 0.35f * cpu_relief_);
    if(delay_mix_eff > 0.005f)
    {
        float delayed_left  = g_delay_left.Read();
        float delayed_right = g_delay_right.Read();
        // Feedback loop con soft clip per evitare instabilità a feedback alto
        g_delay_left.Write(SoftClipSymmetric(wet_left  + delayed_left  * current_delay_feedback_));
        g_delay_right.Write(SoftClipSymmetric(wet_right + delayed_right * current_delay_feedback_));
        wet_left  = FastLinearMix(wet_left,  delayed_left,  delay_mix_eff);
        wet_right = FastLinearMix(wet_right, delayed_right, delay_mix_eff);
    }
#endif

    // APPLICA REVERB (umidità 40-100%) — dopo il delay
#if ENABLE_HUMIDITY_EFFECT
    const float reverb_mix_eff = current_reverb_mix_ * (1.0f - 0.50f * cpu_relief_);
    if(reverb_mix_eff > 0.005f)
    {
        float reverb_out_left, reverb_out_right;
        reverb_ptr_->Process(wet_left, wet_right, &reverb_out_left, &reverb_out_right);
        float humidity_trim = 1.0f - kReverbHumidTrimScale * current_humidity_shape_;
        if(humidity_trim < kReverbHumidTrimMin) humidity_trim = kReverbHumidTrimMin;
        wet_left  = FastLinearMix(wet_left,  reverb_out_left,  reverb_mix_eff) * humidity_trim;
        wet_right = FastLinearMix(wet_right, reverb_out_right, reverb_mix_eff) * humidity_trim;
    }
#endif
    
    // ========================================================================
    // STEP 4: DRY/WET MIX (controllato da encoder rotation)
    // ========================================================================
    // encoder_mix [0.0, 1.0]
    // 0.0 = suono originale (dry)
    // 0.5 = 50/50 mix
    // 1.0 = effetti puri (wet)
    if(external_input)
    {
        float wet_makeup = 1.0f + kExternalWetMakeupMaxBoost * wet_ctrl;
        wet_left *= wet_makeup;
        wet_right *= wet_makeup;
    }

    out_left = EqualPowerMix(dry_left, wet_left, encoder_mix);
    out_right = EqualPowerMix(dry_right, wet_right, encoder_mix);

    // ========================================================================
    // STEP 5: COMPENSAZIONE VOLUME LEGGERA + VOLUME FINALE
    // ========================================================================
    // Keep user volume independent from wet amount.
    float base_output_gain = volume * kOutputMakeupGain;
    float output_gain = base_output_gain
                        * (1.0f + (effect_output_gain_ - 1.0f) * wet_ctrl);
    if(output_gain > kOutputGainMax)
        output_gain = kOutputGainMax;
    // In DEV mode, allow volume to reach true zero. In Ultra mode, clamp to avoid pops.
    if(output_gain < 0.05f && base_output_gain >= 0.05f)
        output_gain = 0.05f;
    out_left *= output_gain;
    out_right *= output_gain;

    out_left  = SanitizeFinite(out_left);
    out_right = SanitizeFinite(out_right);
    
    // ========================================================================
    // STEP 6: HARD CLIPPING (protezione da saturazione + aggressività)
    // ========================================================================
    // Hard clipping simmetrico per una distorsione "fritte" aggressiva
    // Se il segnale supera la soglia, viene tagliato netto
    // Questo crea distorsione pura (suono "fritto")
    // Softer protection near dry settings; stronger clipping only as wet increases.
    const float clip_threshold = 0.995f - 0.045f * wet_ctrl;
    
    out_left = HardClip(out_left, clip_threshold);
    out_right = HardClip(out_right, clip_threshold);
}

// ============================================================================
// SETTERI PARAMETRI
// ============================================================================

void AudioProcessor::SetFilterFrequency(float freq)
{
    // ========================================================================
    // Limita la frequenza in intervallo ragionevole (20Hz → 20kHz)
    // ========================================================================
    if (freq < 20.0f) freq = 20.0f;
    if (freq > 20000.0f) freq = 20000.0f;
    
    current_filter_freq_ = freq;
    filter_left_.SetFreq(freq);
    filter_right_.SetFreq(freq);
}

float AudioProcessor::LightToFilterFreq(float light) const
{
    // Il sensore luce puo' arrivare normalizzato [0..1] oppure in percentuale [0..100].
    float light_percent = (light <= 1.0f) ? (light * 100.0f) : light;
    float light_amount = Clamp01((light_percent - kLightSensorMin)
                                 / (kLightSensorMax - kLightSensorMin));

    static constexpr float kLogRatio = 5.19296f; // ln(kLightFreqMaxHz / kLightFreqMinHz)
    return kLightFreqMinHz * expf(light_amount * kLogRatio);
}

float AudioProcessor::LightToFilterRes(float light) const
{
    float light_percent = (light <= 1.0f) ? (light * 100.0f) : light;
    float light_amount = Clamp01((light_percent - kLightSensorMin)
                                 / (kLightSensorMax - kLightSensorMin));

    // Reverse: poca luce => resonance alta, molta luce => resonance bassa.
    float reverse_amount = 1.0f - light_amount;
    float shaped = SmoothStep(0.0f, 1.0f, reverse_amount);
    return kLightResMin + (kLightResMax - kLightResMin) * shaped;
}

float AudioProcessor::TemperatureToShape(float temp) const
{
    // Curva a due stadi:
    // 15-38°C: crescita morbida e controllata
    // 38-40°C: aumento deciso dell'intensita'
    if(temp <= 15.0f)
    {
        return 0.0f;
    }

    if(temp >= 40.0f)
    {
        return 1.0f;
    }

    if(temp <= 38.0f)
    {
        float t = SmoothStep(15.0f, 38.0f, temp);
        return 0.58f * t * t;
    }

    float t_hi = Clamp01((temp - 38.0f) * 0.5f);
    return 0.58f + 0.42f * SmoothStep(0.0f, 1.0f, t_hi);
}

float AudioProcessor::HumidityToShape(float humidity) const
{
    if(humidity <= 0.0f)
    {
        return 0.0f;
    }

    if(humidity >= 100.0f)
    {
        return 1.0f;
    }

    return SmoothStep(0.0f, 100.0f, humidity);
}
