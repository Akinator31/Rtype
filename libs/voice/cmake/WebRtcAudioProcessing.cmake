# The audio processing of WebRTC (webrtc-audio-processing 2.1, from PulseAudio): the echo canceller AEC3, the
# gain control AGC2, the noise suppression and the high-pass filter, for the microphone of the voice
# (docs/architecture/voice.md#the-sound-processing). Its build is meson only: the sources it lists for
# Linux and Windows on x86 are compiled here, so that neither meson nor its subproject of abseil is needed. The
# list comes from its meson.build files (rtc_base, api, system_wrappers, common_audio, third_party, modules):
# to be read again at an update.

# abseil, which WebRTC is written against: the release its meson wrap pins
CPMAddPackage(
    NAME abseil
    VERSION 20240722.0
    URL https://github.com/abseil/abseil-cpp/releases/download/20240722.0/abseil-cpp-20240722.0.tar.gz
    URL_HASH SHA256=f50e5ac311a81382da7fa75b97310e4b9006474f9560ac46f54a9967f07d4ae3
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "ABSL_PROPAGATE_CXX_STD ON"
        "ABSL_ENABLE_INSTALL OFF"
        "ABSL_BUILD_TESTING OFF"
        "BUILD_TESTING OFF"
)

CPMAddPackage(
    NAME webrtc_audio_processing
    VERSION 2.1
    URL https://freedesktop.org/software/pulseaudio/webrtc-audio-processing/webrtc-audio-processing-2.1.tar.xz
    URL_HASH SHA256=ae9302824b2038d394f10213cab05312c564a038434269f11dbf68f511f9f9fe
    DOWNLOAD_ONLY YES
)
set(WEBRTC_DIR ${webrtc_audio_processing_SOURCE_DIR}/webrtc)

add_library(rtype_webrtc_apm STATIC
    ${WEBRTC_DIR}/rtc_base/checks.cc
    ${WEBRTC_DIR}/rtc_base/containers/flat_tree.cc
    ${WEBRTC_DIR}/rtc_base/event.cc
    ${WEBRTC_DIR}/rtc_base/event_tracer.cc
    ${WEBRTC_DIR}/rtc_base/experiments/field_trial_parser.cc
    ${WEBRTC_DIR}/rtc_base/logging.cc
    ${WEBRTC_DIR}/rtc_base/memory/aligned_malloc.cc
    ${WEBRTC_DIR}/rtc_base/platform_thread.cc
    ${WEBRTC_DIR}/rtc_base/platform_thread_types.cc
    ${WEBRTC_DIR}/rtc_base/race_checker.cc
    ${WEBRTC_DIR}/rtc_base/random.cc
    ${WEBRTC_DIR}/rtc_base/string_encode.cc
    ${WEBRTC_DIR}/rtc_base/string_to_number.cc
    ${WEBRTC_DIR}/rtc_base/string_utils.cc
    ${WEBRTC_DIR}/rtc_base/strings/string_builder.cc
    ${WEBRTC_DIR}/rtc_base/synchronization/sequence_checker_internal.cc
    ${WEBRTC_DIR}/rtc_base/synchronization/yield_policy.cc
    ${WEBRTC_DIR}/rtc_base/system/file_wrapper.cc
    ${WEBRTC_DIR}/rtc_base/system_time.cc
    ${WEBRTC_DIR}/rtc_base/time_utils.cc
    ${WEBRTC_DIR}/rtc_base/zero_memory.cc
    ${WEBRTC_DIR}/api/audio/audio_frame.cc
    ${WEBRTC_DIR}/api/audio/audio_processing.cc
    ${WEBRTC_DIR}/api/audio/audio_processing_statistics.cc
    ${WEBRTC_DIR}/api/audio/channel_layout.cc
    ${WEBRTC_DIR}/api/audio/echo_canceller3_config.cc
    ${WEBRTC_DIR}/api/rtp_headers.cc
    ${WEBRTC_DIR}/api/rtp_packet_info.cc
    ${WEBRTC_DIR}/api/task_queue/task_queue_base.cc
    ${WEBRTC_DIR}/api/units/frequency.cc
    ${WEBRTC_DIR}/api/units/time_delta.cc
    ${WEBRTC_DIR}/api/units/timestamp.cc
    ${WEBRTC_DIR}/api/video/color_space.cc
    ${WEBRTC_DIR}/api/video/hdr_metadata.cc
    ${WEBRTC_DIR}/api/video/video_content_type.cc
    ${WEBRTC_DIR}/api/video/video_timing.cc
    ${WEBRTC_DIR}/system_wrappers/source/cpu_features.cc
    ${WEBRTC_DIR}/system_wrappers/source/denormal_disabler.cc
    ${WEBRTC_DIR}/system_wrappers/source/field_trial.cc
    ${WEBRTC_DIR}/system_wrappers/source/metrics.cc
    ${WEBRTC_DIR}/system_wrappers/source/sleep.cc
    ${WEBRTC_DIR}/common_audio/audio_converter.cc
    ${WEBRTC_DIR}/common_audio/audio_util.cc
    ${WEBRTC_DIR}/common_audio/channel_buffer.cc
    ${WEBRTC_DIR}/common_audio/fir_filter_c.cc
    ${WEBRTC_DIR}/common_audio/fir_filter_factory.cc
    ${WEBRTC_DIR}/common_audio/resampler/push_resampler.cc
    ${WEBRTC_DIR}/common_audio/resampler/push_sinc_resampler.cc
    ${WEBRTC_DIR}/common_audio/resampler/resampler.cc
    ${WEBRTC_DIR}/common_audio/resampler/sinc_resampler.cc
    ${WEBRTC_DIR}/common_audio/resampler/sinusoidal_linear_chirp_source.cc
    ${WEBRTC_DIR}/common_audio/ring_buffer.c
    ${WEBRTC_DIR}/common_audio/signal_processing/auto_correlation.c
    ${WEBRTC_DIR}/common_audio/signal_processing/auto_corr_to_refl_coef.c
    ${WEBRTC_DIR}/common_audio/signal_processing/complex_bit_reverse.c
    ${WEBRTC_DIR}/common_audio/signal_processing/complex_fft.c
    ${WEBRTC_DIR}/common_audio/signal_processing/copy_set_operations.c
    ${WEBRTC_DIR}/common_audio/signal_processing/cross_correlation.c
    ${WEBRTC_DIR}/common_audio/signal_processing/division_operations.c
    ${WEBRTC_DIR}/common_audio/signal_processing/dot_product_with_scale.cc
    ${WEBRTC_DIR}/common_audio/signal_processing/downsample_fast.c
    ${WEBRTC_DIR}/common_audio/signal_processing/filter_ar.c
    ${WEBRTC_DIR}/common_audio/signal_processing/filter_ar_fast_q12.c
    ${WEBRTC_DIR}/common_audio/signal_processing/energy.c
    ${WEBRTC_DIR}/common_audio/signal_processing/filter_ma_fast_q12.c
    ${WEBRTC_DIR}/common_audio/signal_processing/get_hanning_window.c
    ${WEBRTC_DIR}/common_audio/signal_processing/get_scaling_square.c
    ${WEBRTC_DIR}/common_audio/signal_processing/ilbc_specific_functions.c
    ${WEBRTC_DIR}/common_audio/signal_processing/levinson_durbin.c
    ${WEBRTC_DIR}/common_audio/signal_processing/lpc_to_refl_coef.c
    ${WEBRTC_DIR}/common_audio/signal_processing/min_max_operations.c
    ${WEBRTC_DIR}/common_audio/signal_processing/randomization_functions.c
    ${WEBRTC_DIR}/common_audio/signal_processing/real_fft.c
    ${WEBRTC_DIR}/common_audio/signal_processing/refl_coef_to_lpc.c
    ${WEBRTC_DIR}/common_audio/signal_processing/resample_48khz.c
    ${WEBRTC_DIR}/common_audio/signal_processing/resample_by_2.c
    ${WEBRTC_DIR}/common_audio/signal_processing/resample_by_2_internal.c
    ${WEBRTC_DIR}/common_audio/signal_processing/resample.c
    ${WEBRTC_DIR}/common_audio/signal_processing/resample_fractional.c
    ${WEBRTC_DIR}/common_audio/signal_processing/spl_init.c
    ${WEBRTC_DIR}/common_audio/signal_processing/spl_inl.c
    ${WEBRTC_DIR}/common_audio/signal_processing/splitting_filter.c
    ${WEBRTC_DIR}/common_audio/signal_processing/spl_sqrt.c
    ${WEBRTC_DIR}/common_audio/signal_processing/sqrt_of_one_minus_x_squared.c
    ${WEBRTC_DIR}/common_audio/signal_processing/vector_scaling_operations.c
    ${WEBRTC_DIR}/common_audio/smoothing_filter.cc
    ${WEBRTC_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft.cc
    ${WEBRTC_DIR}/common_audio/third_party/ooura/fft_size_256/fft4g.cc
    ${WEBRTC_DIR}/common_audio/third_party/spl_sqrt_floor/spl_sqrt_floor.c
    ${WEBRTC_DIR}/common_audio/vad/vad.cc
    ${WEBRTC_DIR}/common_audio/vad/vad_core.c
    ${WEBRTC_DIR}/common_audio/vad/vad_filterbank.c
    ${WEBRTC_DIR}/common_audio/vad/vad_gmm.c
    ${WEBRTC_DIR}/common_audio/vad/vad_sp.c
    ${WEBRTC_DIR}/common_audio/vad/webrtc_vad.c
    ${WEBRTC_DIR}/third_party/rnnoise/src/rnn_vad_weights.cc
    ${WEBRTC_DIR}/modules/third_party/fft/fft.c
    ${WEBRTC_DIR}/modules/audio_coding/codecs/isac/main/source/filter_functions.c
    ${WEBRTC_DIR}/modules/audio_coding/codecs/isac/main/source/isac_vad.c
    ${WEBRTC_DIR}/modules/audio_coding/codecs/isac/main/source/pitch_estimator.c
    ${WEBRTC_DIR}/modules/audio_coding/codecs/isac/main/source/pitch_filter.c
    ${WEBRTC_DIR}/modules/audio_processing/aec_dump/null_aec_dump_factory.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/adaptive_fir_filter.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/adaptive_fir_filter_erl.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/aec3_common.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/aec3_fft.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/aec_state.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/alignment_mixer.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/api_call_jitter_metrics.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/block_buffer.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/block_delay_buffer.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/block_framer.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/block_processor.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/block_processor_metrics.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/clockdrift_detector.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/coarse_filter_update_gain.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/comfort_noise_generator.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/config_selector.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/decimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/dominant_nearend_detector.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/downsampled_render_buffer.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/echo_audibility.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/echo_canceller3.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/echo_path_delay_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/echo_path_variability.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/echo_remover.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/echo_remover_metrics.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/erle_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/erl_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/fft_buffer.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/filter_analyzer.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/frame_blocker.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/fullband_erle_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/matched_filter.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/matched_filter_lag_aggregator.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/moving_average.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/multi_channel_content_detector.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/refined_filter_update_gain.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/render_buffer.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/render_delay_buffer.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/render_delay_controller.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/render_delay_controller_metrics.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/render_signal_analyzer.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/residual_echo_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/reverb_decay_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/reverb_frequency_response.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/reverb_model.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/reverb_model_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/signal_dependent_erle_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/spectrum_buffer.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/stationarity_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/subband_erle_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/subband_nearend_detector.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/subtractor.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/subtractor_output_analyzer.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/subtractor_output.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/suppression_filter.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/suppression_gain.cc
    ${WEBRTC_DIR}/modules/audio_processing/aec3/transparent_mode.cc
    ${WEBRTC_DIR}/modules/audio_processing/aecm/aecm_core.cc
    ${WEBRTC_DIR}/modules/audio_processing/aecm/echo_control_mobile.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc/agc.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc/agc_manager_direct.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc/legacy/analog_agc.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc/legacy/digital_agc.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc/loudness_histogram.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc/utility.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/adaptive_digital_gain_controller.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/agc2_testing_common.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/biquad_filter.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/clipping_predictor.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/clipping_predictor_level_buffer.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/compute_interpolated_gain_curve.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/cpu_features.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/fixed_digital_level_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/gain_applier.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/input_volume_controller.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/input_volume_stats_reporter.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/interpolated_gain_curve.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/limiter.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/limiter_db_gain_curve.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/noise_level_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/rnn_vad/auto_correlation.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/rnn_vad/features_extraction.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/rnn_vad/lp_residual.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/rnn_vad/pitch_search.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/rnn_vad/pitch_search_internal.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/rnn_vad/rnn.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/rnn_vad/rnn_fc.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/rnn_vad/rnn_gru.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/rnn_vad/spectral_features.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/rnn_vad/spectral_features_internal.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/saturation_protector.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/saturation_protector_buffer.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/speech_level_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/speech_probability_buffer.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/vad_wrapper.cc
    ${WEBRTC_DIR}/modules/audio_processing/agc2/vector_float_frame.cc
    ${WEBRTC_DIR}/modules/audio_processing/audio_buffer.cc
    ${WEBRTC_DIR}/modules/audio_processing/audio_processing_builder_impl.cc
    ${WEBRTC_DIR}/modules/audio_processing/audio_processing_impl.cc
    ${WEBRTC_DIR}/modules/audio_processing/capture_levels_adjuster/audio_samples_scaler.cc
    ${WEBRTC_DIR}/modules/audio_processing/capture_levels_adjuster/capture_levels_adjuster.cc
    ${WEBRTC_DIR}/modules/audio_processing/echo_control_mobile_impl.cc
    ${WEBRTC_DIR}/modules/audio_processing/echo_detector/circular_buffer.cc
    ${WEBRTC_DIR}/modules/audio_processing/echo_detector/mean_variance_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/echo_detector/moving_max.cc
    ${WEBRTC_DIR}/modules/audio_processing/echo_detector/normalized_covariance_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/gain_control_impl.cc
    ${WEBRTC_DIR}/modules/audio_processing/gain_controller2.cc
    ${WEBRTC_DIR}/modules/audio_processing/high_pass_filter.cc
    ${WEBRTC_DIR}/modules/audio_processing/include/aec_dump.cc
    ${WEBRTC_DIR}/modules/audio_processing/include/audio_frame_proxies.cc
    ${WEBRTC_DIR}/modules/audio_processing/logging/apm_data_dumper.cc
    ${WEBRTC_DIR}/modules/audio_processing/ns/fast_math.cc
    ${WEBRTC_DIR}/modules/audio_processing/ns/histograms.cc
    ${WEBRTC_DIR}/modules/audio_processing/ns/noise_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/ns/noise_suppressor.cc
    ${WEBRTC_DIR}/modules/audio_processing/ns/ns_fft.cc
    ${WEBRTC_DIR}/modules/audio_processing/ns/prior_signal_model.cc
    ${WEBRTC_DIR}/modules/audio_processing/ns/prior_signal_model_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/ns/quantile_noise_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/ns/signal_model.cc
    ${WEBRTC_DIR}/modules/audio_processing/ns/signal_model_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/ns/speech_probability_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/ns/suppression_params.cc
    ${WEBRTC_DIR}/modules/audio_processing/ns/wiener_filter.cc
    ${WEBRTC_DIR}/modules/audio_processing/residual_echo_detector.cc
    ${WEBRTC_DIR}/modules/audio_processing/rms_level.cc
    ${WEBRTC_DIR}/modules/audio_processing/splitting_filter.cc
    ${WEBRTC_DIR}/modules/audio_processing/three_band_filter_bank.cc
    ${WEBRTC_DIR}/modules/audio_processing/utility/cascaded_biquad_filter.cc
    ${WEBRTC_DIR}/modules/audio_processing/utility/delay_estimator.cc
    ${WEBRTC_DIR}/modules/audio_processing/utility/delay_estimator_wrapper.cc
    ${WEBRTC_DIR}/modules/audio_processing/utility/pffft_wrapper.cc
    ${WEBRTC_DIR}/modules/audio_processing/vad/gmm.cc
    ${WEBRTC_DIR}/modules/audio_processing/vad/pitch_based_vad.cc
    ${WEBRTC_DIR}/modules/audio_processing/vad/pitch_internal.cc
    ${WEBRTC_DIR}/modules/audio_processing/vad/pole_zero_filter.cc
    ${WEBRTC_DIR}/modules/audio_processing/vad/standalone_vad.cc
    ${WEBRTC_DIR}/modules/audio_processing/vad/vad_audio_proc.cc
    ${WEBRTC_DIR}/modules/audio_processing/vad/vad_circular_buffer.cc
    ${WEBRTC_DIR}/modules/audio_processing/vad/voice_activity_detector.cc
    ${WEBRTC_DIR}/common_audio/signal_processing/complex_bit_reverse.c
    ${WEBRTC_DIR}/common_audio/signal_processing/filter_ar_fast_q12.c
    ${WEBRTC_DIR}/modules/audio_processing/aecm/aecm_core_c.cc
    ${WEBRTC_DIR}/third_party/pffft/src/pffft.c
)
target_include_directories(rtype_webrtc_apm SYSTEM PUBLIC ${WEBRTC_DIR})
# Not our code: its warnings (a switch on every value that returns nothing after it) are not ours
target_compile_options(rtype_webrtc_apm PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>)
# The parts of abseil its sources include: not its flags nor its synchronization, which its meson.build names
# but nothing uses, and whose containers GCC refuses in a constant expression under the sanitizers
target_link_libraries(rtype_webrtc_apm PUBLIC absl::base absl::core_headers absl::strings absl::algorithm
                      absl::meta absl::any_invocable)
target_compile_definitions(rtype_webrtc_apm
    PUBLIC WEBRTC_APM_DEBUG_DUMP=0 WEBRTC_LIBRARY_IMPL
    PRIVATE NDEBUG _WINSOCKAPI_)
if(WIN32)
  target_compile_definitions(rtype_webrtc_apm PUBLIC WEBRTC_WIN NOMINMAX _USE_MATH_DEFINES)
  target_link_libraries(rtype_webrtc_apm PRIVATE winmm)
else()
  target_compile_definitions(rtype_webrtc_apm PUBLIC WEBRTC_POSIX WEBRTC_LINUX)
  find_package(Threads REQUIRED)
  target_link_libraries(rtype_webrtc_apm PRIVATE Threads::Threads)
endif()
set_source_files_properties(${WEBRTC_DIR}/third_party/pffft/src/pffft.c PROPERTIES COMPILE_DEFINITIONS _GNU_SOURCE)
# rtc_base/trace_event.h uses uint8_t without <cstdint>, which recent GCC no longer includes through <string>:
# included before each C++ file, the sources left as they are
if(MSVC)
  target_compile_options(rtype_webrtc_apm PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/FIcstdint>)
else()
  target_compile_options(rtype_webrtc_apm PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cstdint>")
endif()

# The parts written for SSE2 and AVX2, chosen at run time by the CPU: AVX2 only where it is compiled for it
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
  target_compile_definitions(rtype_webrtc_apm PUBLIC WEBRTC_ENABLE_AVX2)
  set(WEBRTC_SSE2_SOURCES
      ${WEBRTC_DIR}/common_audio/fir_filter_sse.cc
      ${WEBRTC_DIR}/common_audio/resampler/sinc_resampler_sse.cc
      ${WEBRTC_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft_sse2.cc)
  set(WEBRTC_AVX2_SOURCES
      ${WEBRTC_DIR}/common_audio/fir_filter_avx2.cc
      ${WEBRTC_DIR}/common_audio/resampler/sinc_resampler_avx2.cc
      ${WEBRTC_DIR}/modules/audio_processing/aec3/adaptive_fir_filter_avx2.cc
      ${WEBRTC_DIR}/modules/audio_processing/aec3/adaptive_fir_filter_erl_avx2.cc
      ${WEBRTC_DIR}/modules/audio_processing/aec3/fft_data_avx2.cc
      ${WEBRTC_DIR}/modules/audio_processing/aec3/matched_filter_avx2.cc
      ${WEBRTC_DIR}/modules/audio_processing/aec3/vector_math_avx2.cc
      ${WEBRTC_DIR}/modules/audio_processing/agc2/rnn_vad/vector_math_avx2.cc)
  target_sources(rtype_webrtc_apm PRIVATE ${WEBRTC_SSE2_SOURCES} ${WEBRTC_AVX2_SOURCES})
  if(MSVC)
    set_source_files_properties(${WEBRTC_AVX2_SOURCES} PROPERTIES COMPILE_OPTIONS /arch:AVX2)
  else()
    set_source_files_properties(${WEBRTC_SSE2_SOURCES} PROPERTIES COMPILE_OPTIONS -msse2)
    set_source_files_properties(${WEBRTC_AVX2_SOURCES} PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
  endif()
else()
  target_compile_definitions(rtype_webrtc_apm PUBLIC WAP_DISABLE_INLINE_SSE)
  set_source_files_properties(${WEBRTC_DIR}/third_party/pffft/src/pffft.c PROPERTIES COMPILE_DEFINITIONS
                              "_GNU_SOURCE;PFFFT_SIMD_DISABLE")
endif()

# Its license goes with the client: BSD, as the code of abseil it is built with is Apache 2.0
install(FILES ${webrtc_audio_processing_SOURCE_DIR}/COPYING DESTINATION licenses RENAME webrtc-audio-processing.txt
        COMPONENT client)
install(FILES ${abseil_SOURCE_DIR}/LICENSE DESTINATION licenses RENAME abseil.txt COMPONENT client)
