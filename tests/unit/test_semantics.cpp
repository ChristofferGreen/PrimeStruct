#include <algorithm>
#include <cstdint>

#include "primec/testing/SemanticsValidationHelpers.h"

#include "third_party/doctest.h"
#include "test_semantics_helpers.h"

#include "test_semantics_bindings_core.h"
#include "test_semantics_bindings_struct_defaults.h"
#include "test_semantics_bindings_struct_defaults_maps.h"
#include "test_semantics_bindings_pointers.h"
#include "test_semantics_bindings_assignments.h"
#include "test_semantics_bindings_control_flow.h"
#include "test_semantics_bindings_control_flow_borrowing.h"
#include "test_semantics_calls_and_flow_control_core.h"
#include "test_semantics_calls_and_flow_control_misc.h"
#include "test_semantics_calls_and_flow_collections.h"
#include "test_semantics_calls_and_flow_access.h"
#include "test_semantics_calls_and_flow_access_indexing.h"
#include "test_semantics_calls_and_flow_named_args.h"
#include "test_semantics_calls_and_flow_numeric_builtins.h"
#include "test_semantics_calls_and_flow_comparisons_literals.h"
