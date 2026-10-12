/**
 * ============================================================================
 * ULTRASUONO - Audio Effects Processor
 * ============================================================================
 * 
 * File: src/AudioProcessor.h
 * 
 * DESCRIZIONE:
 * Gestisce il processamento audio in tempo reale.
 * Ogni effetto è controllato da un sensore ambientale.
 * 
 * EFFETTI IMPLEMENTATI:
 * - Luminosità                   → LadderFilter lowpass: cutoff + resonance
 * - Temperatura fredda (-20→12)  → Comb filter stereo + comb tonale + Flanger
 * - Temperatura calda (12→40)    → Overdrive + TubeSat + SoftClip + DiodeClip in serie
 * - Umidità (0→40%)              → Gate ibrido wideband + multiband
 * - Umidità (40→100%)            → Delay stereo (SDRAM, 500ms max) + Reverb (daisysp::ReverbSc)
 * 
 * ARCHITETTURA:
 * AudioProcessor possiede gli effetti DaisySP
 * main.cpp passa dati ambientali e campioni audio
 * AudioProcessor ritorna campioni processati
 * 
 * ============================================================================
 */

#pragma once

#include "daisy_seed.h"
#include "daisysp.h"


/**
 * Classe AudioProcessor
 * 
 * Gestisce il processamento audio con effetti controllati da sensori.
 * Ogni sensore ambientale modula uno o più effetti.
 */
class AudioProcessor
{
public:
    // ========================================================================
    // COSTRUTTORE
    // ========================================================================
    
    /**
     * Costruttore di AudioProcessor
     * Non fa molto: il lavoro è fatto da Init()
     */
    AudioProcessor() {}

    // ========================================================================
    // INIZIALIZZAZIONE
    // ========================================================================
    
    /**
     * Init() - Inizializza tutti gli effetti
     * 
     * DEVE essere chiamato UNA VOLTA all'avvio, prima di usare Process()
     * 
     * Chiama l'Init di ogni effetto DaisySP:
     * - Filtro Lowpass (per il cutoff)
     * - Overdrive (saturation + distorsione)
     * 
     * PARAMETRO: sample_rate = frequenza di campionamento (es. 48000 Hz)
     */
    void Init(float sample_rate);

    // ========================================================================
    // PROCESSAMENTO AUDIO
    // ========================================================================
    
    /**
     * Process() - Processa un campione stereo con tutti gli effetti
     * 
     * PARAMETRI:
     * - in_left, in_right = campioni di ingresso (stereo)
     * - light = valore sensore luce [0.0, 1.0]
     * - temperature = temperatura in°C
    * - humidity = umidità relativa [%]
     * - encoder_mix = dry/wet mix controllato da encoder [0.0, 1.0]
     * - rec_active = se true, il segnale sarà registrato
     * 
     * RITORNA:
     * - out_left, out_right = campioni processati (passati per referenza)
     * 
     * COSA FA INTERNAMENTE:
    * 1. Calcola frequenza cutoff dal valore luce (50Hz → 8kHz)
    * 2. Calcola i parametri temperatura per la catena timbrica
    * 3. Applica il filtro lowpass di luminosità
    * 4. Applica Overdrive + Wavefolder + LadderFilter in modo graduale
    * 5. Mischia dry/wet in base all'encoder
    * 6. Applica volume finale controllato da knob2
    * 7. Hard clipping di sicurezza
     * 
     * NOTE:
     * - Chiamata PER OGNI CAMPIONE AUDIO (~48000 volte/secondo)
     * - Deve essere VELOCE
     * - Non deve fare allocazioni memoria
     */
    void Process(
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
    );

    // ========================================================================
    // SETTERI PARAMETRI (per debug/menu futuro)
    // ========================================================================
    
    /**
     * SetFilterFrequency(float freq) - Imposta manualmente la frequenza cutoff
     * PARAMETRO: freq in Hz (es. 1000.0 = 1kHz)
     * INTERVALLO: ~20Hz a ~20kHz
     */
    void SetFilterFrequency(float freq);

private:
    // ========================================================================
    // EFFETTI DAISYSP
    // ========================================================================
    
    /**
     * Filtro Moog ladder principale per il controllo di luminosita'.
     * - Bassa luce  -> cutoff basso, resonance alta
     * - Alta luce   -> cutoff alto, resonance bassa
     */
    daisysp::LadderFilter   filter_left_;
    daisysp::LadderFilter   filter_right_;

    // Catena di distorsione controllata dalla temperatura.
    daisysp::Overdrive    overdrive_;
    static constexpr size_t kCombDelayMaxSamples = 2048;
    daisysp::DelayLine<float, kCombDelayMaxSamples> comb_left_;
    daisysp::DelayLine<float, kCombDelayMaxSamples> comb_right_;
    static constexpr size_t kColdToneDelayMaxSamples = 2048;
    daisysp::DelayLine<float, kColdToneDelayMaxSamples> cold_tone_left_;
    daisysp::DelayLine<float, kColdToneDelayMaxSamples> cold_tone_right_;

    // Reverb Schroeder ottimizzato (DaisySP) — aux_[98936] (~386KB) vive in SDRAM via puntatore.
    daisysp::ReverbSc* reverb_ptr_;

    // Flanger stereo per la temperatura fredda (-20→12°C).
    // Due istanze mono (DaisySP Flanger è mono-in/out) — ~8KB totali in SRAM interna.
    daisysp::Flanger flanger_left_;
    daisysp::Flanger flanger_right_;

    // Delay stereo per l'umidità alta (40-100%).
    // I buffer (96KB/canale) vivono in SDRAM (dichiarati in AudioProcessor.cpp).
    // Qui teniamo solo indice e parametri correnti.


    // ========================================================================
    // VARIABILI DI STATO
    // ========================================================================
    
    float sample_rate_;              // Sample rate in Hz (48000)
    float input_dc_block_coeff_;     // ~20 Hz input high-pass coefficient
    float input_prev_l_;
    float input_prev_r_;
    float input_dc_state_l_;
    float input_dc_state_r_;
    float current_filter_freq_;      // Frequenza cutoff Moog attuale
    float current_filter_res_;       // Risonanza Moog attuale
    float current_comb_mix_;         // Mix comb temperatura fredda [0, 1]
    float current_comb_delay_;       // Delay comb corrente in campioni
    float current_comb_feedback_;    // Feedback comb corrente [0, 1]
    float current_cold_tone_mix_;    // Mix cold comb tonale [0, 1]
    float current_cold_tone_delay_;  // Delay cold comb tonale in campioni
    float current_cold_tone_feedback_; // Feedback cold comb tonale [0, 1]
    float current_temp_shape_;       // Intensita' generale temperatura [0, 1]
    float current_overdrive_drive_;  // Drive overdrive corrente [0, 1]
    float current_humidity_shape_;   // Intensita' umidita' [0, 1]
    float current_reverb_feedback_;  // Feedback reverb [0, 1]
    float current_reverb_mix_;       // Mix reverb [0, 1]
    float current_reverb_damp_;      // Damping reverb [0, 1]
    float current_temp_gain_comp_;   // Attenuazione dedicata alla sezione temperatura
    float current_temp_shape_curve_; // Quadrato di current_temp_shape_ (cached)
    float current_delay_time_;        // Delay time corrente [campioni]
    float current_delay_feedback_;    // Feedback delay [0, 1]
    float current_delay_mix_;         // Mix delay [0, 1]
    float current_flanger_feedback_;  // Feedback flanger [0, 1]
    float current_flanger_rate_;      // Frequenza LFO flanger [Hz]
    float current_flanger_depth_;     // Profondità modulazione flanger [0, 0.93]
    float current_flanger_mix_;       // Mix flanger [0, 1]
    float cpu_relief_;                // adaptive callback load relief [0, 1]

    // Target aggiornati a control-rate interno per ridurre il carico CPU.
    uint32_t control_update_counter_;
    float light_bypass_;
    float dry_gate_amount_;
    float effect_output_gain_;
    float target_filter_freq_;
    float target_filter_res_;
    float target_temp_shape_;
    float target_comb_mix_;
    float target_comb_delay_;
    float target_comb_feedback_;
    float target_cold_tone_mix_;
    float target_cold_tone_delay_;
    float target_cold_tone_feedback_;
    float target_overdrive_drive_;
    float target_serial_soft_mix_;
    float target_serial_tube_mix_;
    float target_serial_diode_mix_;
    float target_humidity_shape_;
    float target_reverb_feedback_;
    float target_reverb_mix_;
    float target_delay_time_;
    float target_delay_feedback_;
    float target_delay_mix_;
    float target_flanger_feedback_;
    float target_flanger_rate_;
    float target_flanger_depth_;
    float target_flanger_mix_;

    // ========================================================================
    // FUNZIONI PRIVATE - MAPPATURA SENSORI → PARAMETRI
    // ========================================================================
    
    /**
     * LightToFilterFreq(float light) - Converte luminosità in frequenza cutoff
     * 
     * MAPPATURA:
     * - light 0.0 (buio)     → 50 Hz (molto basso, "scuro")
     * - light 0.5 (normale)  → 2000 Hz (mid)
     * - light 1.0 (luminoso) → 8000 Hz (alto, "brillante")
     * 
     * FORMULA: logaritmica per suonare naturale all'orecchio
     */
    float LightToFilterFreq(float light) const;

    /**
     * LightToFilterRes(float light) - Converte luminosita' in resonance del ladder.
     */
    float LightToFilterRes(float light) const;

    /**
     * TemperatureToShape(float temp) - Curva lineare temperatura [20°C, 40°C].
     */
    float TemperatureToShape(float temp) const;

    /**
     * HumidityToShape(float humidity) - Curva lineare umidita' [0%, 100%].
     */
    float HumidityToShape(float humidity) const;

};
