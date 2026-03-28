#include <cstring>
#include <string>
#include <vector>

#include "third_party/doctest.h"

#include "primec/Ast.h"
#include "primec/Ir.h"
#include "primec/IrLowerer.h"
#include "primec/IrSerializer.h"
#include "primec/Vm.h"
#include "test_ir_pipeline_helpers.h"

#include "test_ir_pipeline_conversions_core.h"
#include "test_ir_pipeline_conversions_variadic_collection_refs.h"
#include "test_ir_pipeline_conversions_variadic_pointer_refs.h"
#include "test_ir_pipeline_conversions_variadic_field_refs_and_maps.h"
#include "test_ir_pipeline_conversions_variadic_pointer_maps.h"
#include "test_ir_pipeline_conversions_variadic_pointer_vectors.h"

TEST_SUITE_END();
