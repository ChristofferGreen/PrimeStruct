#include "test_compile_run_helpers.h"

#include <cerrno>

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");

#include "test_compile_run_native_backend_file_and_struct_variadics.h"
#include "test_compile_run_native_backend_result_and_vector_variadics.h"
#include "test_compile_run_native_backend_array_and_pointer_variadics.h"
#include "test_compile_run_native_backend_reference_and_uninitialized_variadics.h"
#include "test_compile_run_native_backend_map_and_vector_variadics.h"
#include "test_compile_run_native_backend_vector_and_experimental_map_variadics.h"
#include "test_compile_run_native_backend_buffer_and_collection_wrappers.h"
#include "test_compile_run_native_backend_image_io_a.h"
#include "test_compile_run_native_backend_image_io_b.h"
#include "test_compile_run_native_backend_image_io_c.h"
#include "test_compile_run_native_backend_image_io_d.h"
#include "test_compile_run_native_backend_runtime_and_ir_paths.h"
#include "test_compile_run_native_backend_result_payloads_and_strings.h"
#include "test_compile_run_native_backend_error_and_file_variadics.h"
#include "test_compile_run_native_backend_ui_layout_a.h"
#include "test_compile_run_native_backend_ui_layout_b.h"

TEST_SUITE_END();
#endif
