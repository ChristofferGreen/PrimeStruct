# VM Inspector: Plan

Classification: **design direction**. Nothing here is implemented and no `docs/todo.md` entries are filed yet; the
slices in section 12 become entries as the open decisions in section 13 are made. It is a companion to
`docs/AiWorkflowPlan.md`: that plan lets a person read a project's code quickly, this one lets them see what the
code does when it runs.

## 1. Goal

A program running on the VM should be open to inspection the way a Smalltalk image is: its objects, its stacks, its
heap and its tasks are visible in a PrimeScroll window, and every value can be followed, watched and traced back to
the code that wrote it.

The window is built for a reviewer, so it is organized around three questions and not around the VM's structure:

1. **What did this code just do?** The values an event or a call changed, before and after.
2. **Why does this value have this content?** Its history, and the line that set it.
3. **Where did the memory go?** Who owns what, what nothing owns, and how it is laid out.

Two things make this stronger than a Smalltalk inspector. The VM is deterministic and can replay a recorded run, so
the window can go backwards in time. And the language has ownership, so the objects of a program form a tree from a
few roots instead of a free graph.

## 2. What exists today

Checked in the sources on 2026-11-10.

| Piece | Where | State |
| --- | --- | --- |
| Paused state | `VmDebugSnapshotPayload` (`include/primec/runtime/Vm.h`) | Call stack, every frame's locals and the operand stack, as raw `u64` values |
| Local names and types | `IrLocalDebugSlot` (`include/primec/ir/Ir.h`) | Slot index, name and type name per function; no range in which the local is live |
| Struct layouts | `IrStructLayout` | Field name, offset, size, alignment, padding kind, visibility, static flag |
| Source maps | `IrInstructionSourceMapEntry` | Line, column, source unit and provenance per instruction |
| Breakpoints | `VmDebugSession::addBreakpoint`, `VmDebugAdapter::setSourceBreakpoints` | By instruction and by source line; a line can resolve to several instructions |
| Stepping | `VmDebugAdapter::step` | One step; DAP `next` maps to it. No step over, into or out |
| Variables | `VmDebugAdapter::variables` | Two flat scopes per frame (locals, operand stack); a struct value does not expand into fields |
| Tasks | `docs/spec/vm-design.md`, Tasks | The VM runs all tasks on one OS thread with one shared heap. **A debug session faults on task opcodes**, and DAP reports one thread |
| Heap | `VmHeapCore` | Address is `bit 63 \| generation \| slot * 16`; a live allocation never moves; freed slots are zeroed and reused under a new generation; free runs are tracked; byte regions are tracked per 64-byte granule. A block has a size but no type |
| Frames | `docs/spec/vm-design.md`, Frames & stack | Locals in 16-byte slots; a local address is numbered across the live frames of one call stack |
| Writes | `StoreLocal`, `StoreIndirect`, `StoreU8` | Every write to a local or to memory goes through one of three opcodes |
| Protocol | `primevm --debug-dap`, `--debug-json`, `--debug-trace`, `--debug-replay`, `--debug-replay-sequence` | DAP on stdio, JSON events, trace recording and replay to a sequence number |
| Optimization | `docs/spec/source-pipeline.md` | Debug sessions and traces use `-O0`; optimized code removes locals |

The main gap is that heap memory has no types at run time. Section 3 says how the window gets them back.

## 3. Types for untyped memory

A Smalltalk object carries its class. A PrimeStruct heap block is a count of slots. Two sources together give every
block a type and an owner:

- **The typed walk.** Start at the roots, which are typed: the locals of every live frame and the `[static]` fields
  (the language's only global state; top-level bindings are rejected). Follow struct fields through the layouts, and
  `Pointer<T>`, `Reference<T>`, `Vector<T>`, `Map<K, V>` and `String` through their known shapes. The result is the
  ownership tree. A live block the walk does not reach has **no owner**: a leak, or memory held only through a raw
  pointer.
- **The allocation table.** In a debug run the VM records, per live block, the function and instruction that
  allocated it and the sequence number. It is a side table, so the heap's layout is the same with and without it. It
  answers "created by", and it names a block the walk cannot type.

An **object** in this plan is a struct instance, wherever it lives: in a heap block of its own, embedded in another
struct or in a vector's storage, or in a stack frame.

## 4. The window

```
+------------------------------------------------------------------+
| run controls   why it stopped             Objects | Memory  Find |
+----------------------+-------------------------------------------+
| Show v   Group by v  |  selected item                            |
|                      |  owner path                               |
| tree                 |                                           |
|  Breakpoints         |  content, by kind of item:                |
|  namespace           |   object    fields, layout, history       |
|   struct             |   struct    table of its objects          |
|    object            |   function  code with breakpoint gutter   |
|   function           |   stack     frames and slots              |
|    local             |   heap      block map and owners          |
|  No owner            |                                           |
+----------------------+-------------------------------------------+
| events: Launch | Open main.prime | Key "f" | Save | Key "n"      |
+------------------------------------------------------------------+
```

- **Left: one tree.** It looks and behaves like a file browser list: disclosure arrows, columns for count and size
  that add up at each level. It is the only navigation; there is one selection, and the right side shows it.
- **Right: the selected item.** What is drawn depends on the kind of item (sections 6 to 9).
- **Top: run controls**, the reason the program is stopped in words ("`dirty` changed from false to true at line
  409"), the switch between the Objects and Memory modes of the tree, and a Find field.
- **Bottom: time**, as a row of named events (section 10).
- **Problems come to the user.** A leak is a badge in the toolbar, a row in the tree and a mark on the timeline; the
  user does not have to look for it.

The window is drawn with the display language of `docs/AiWorkflowPlan.md` section 4.2, and its tree is the same
hierarchy as the box view (namespace, struct, definition), so a function or struct selected here can be opened
there and the other way round. The run-time lens of that plan (section 5.8) reads the same data.

The program under inspection runs in its own process (`primevm --debug-dap`), and PrimeScroll is the client. A
crash or a hang in the program cannot take the editor down, and startup time of the editor is not affected until a
session is started.

## 5. What the tree shows

What appears is selectable. The default is state only.

| Kind | What it is at run time | Default |
| --- | --- | --- |
| Namespaces | Folders; they hold no state themselves | On |
| Structs with live objects | One row per struct, with count and size | On |
| Objects | Struct instances (section 3) | On |
| Static fields | `[static]` struct fields | On |
| Closures | Stateful lambdas with their captured values | On |
| Host resources | Open files, sockets, windows and native widgets, folder watchers | On, as one "Resources" group |
| No owner | Live blocks the typed walk does not reach | On when present |
| Breakpoints | Every breakpoint and watch (section 8) | On when present |
| Structs with no live objects | Types the program defines but does not use now | Off |
| Functions | Definitions, marked when on a stack; parameters and locals below | Off |
| Library (`std`) | Vectors, maps, strings and UI types as rows of their own | Off |
| Template instances | `Vector<i32>`, `Vector<Tab>` as separate rows | Off; grouped under the one struct or function when on |
| Raw memory | Untyped blocks, byte regions, free space, the module string table | Off; this is the Memory mode |

Numbers, booleans, enum values and short strings are never rows. They are fields.

Rules:

- **Size is retained size.** An object's size includes everything it owns. A `Document` row includes its text and
  its vectors, so hiding the library loses no memory: it is counted where the user thinks of it.
- **A collection is a field, not an object.** `tabs` shows inside `EditorState`; it is not also a row "Vector".
- **Large groups do not expand in the tree.** Above about 20 objects a struct row shows the first few and the total;
  selecting the struct opens a sortable table on the right (section 6).
- **Marks roll up.** A row changed by the current event, a created or destroyed object, a breakpoint and a watch each
  have a mark, and a parent carries the marks of its children.
- **Find searches hidden kinds too** and says where the results are ("3 results in Library").

Two controls in the tree's header:

- **Show:** the switches of the table, with three presets on top: "State" (default), "State and code",
  "Everything".
- **Group by:** "Type" (namespace, struct, object), "Owner" (the ownership tree from the roots), or "Task" (the
  task whose stack roots the object, and on `optexe` the arena that holds it).

### Object names

Objects have no names. The label of an object is, in order: the value of the field its struct names as its label
(a transform on the struct; open, section 13), else its first `String` field, else its owner path
(`state.tabs[1].document`). A closure is labelled by the function that created it and the names of its captures.

## 6. Objects and structs

**An object** shows:

- its **owner path**, each step a link (`state > tabs > Tab main.prime > document`);
- its **fields as rows, value first**. A field the current event changed shows the old value struck out and the new
  one marked. A field that is a struct or a collection opens in place or by selection;
- a **Layout** tab: the bytes of the struct as a strip, one segment per field, padding in grey, with the count of
  padding bytes;
- for the selected field, its **history**: every value it had, the function that set it, and a jump to that moment;
  and **Set at**, the source lines around the last write.

**A struct** shows its objects as a **table**: one row per object, one column per field, sortable, with changed
cells marked. Three objects and three thousand read the same way.

**Changes** is a view of its own over the same data: the list of all values the current event (or call, or line)
changed, before and after, across objects. It is the direct answer to question 1 of section 1 and is where the
window opens when the program stops at an event.

## 7. Functions and locals

With functions switched on, a function sits under its namespace, a struct helper under its struct.

- **Selecting a function shows its code.** Values appear beside the lines that produced them.
- **A function expands to its parameters and locals.** A local is a declaration first: when the function is not
  running, its locals are listed with types and no values. When it is on a stack, they show values.
- **More than one activation** (recursion, or two tasks): the row shows the count and lets the user pick; the
  deepest is the default.
- **A local that is not initialized yet** reads "not set yet". This needs the range in which each local is live,
  which the debug slots do not carry today.
- **Templates:** one source function can be several compiled functions. The tree shows one row; the versions are
  listed on demand, and a breakpoint on a line applies to all of them.
- When the program stops, the tree reveals the function it stopped in and the code view marks the line.

Long function lists need a filter in the tree, or it stops being readable once functions are on.

## 8. Breakpoints and watches

**Line breakpoints.** A click in the code view's gutter. A line without code snaps to the next line that has some.
The function row's menu adds **Break on entry** and **Break on return**; the second shows the result value.

**Watches.** The menu of a local, a field or an object:

- **Break on change** (the value differs) and **Break on any write**;
- **Log changes:** record each change on the timeline without stopping;
- **Show history** and **Go to previous change**;
- **Break when...** with a condition (later; it needs an expression evaluator).

A watch works in one of three ways, by its target:

| Target | Mechanism | Lasts |
| --- | --- | --- |
| A local of a function | The `StoreLocal` instructions to its slot are known at compile time: a set of breakpoints that fire when the value differs | Across calls and runs |
| A field of one object | A watched address range, checked in `StoreIndirect` and `StoreU8` | Until the object is destroyed |
| A field of every object of a struct | The compiler tags the stores that assign the field: a set of breakpoints again | Across runs |

Details:

- A local whose address is taken can also be written through a reference; while its frame is live it needs the
  address watch too.
- A live allocation never moves, but a struct in a vector's storage gets a new address when the vector is
  reallocated. `HeapRealloc` and move must carry a watch to the new address.
- The heap's generation shows when a watched object is freed. The watch ends with a notice.
- A write through a raw pointer passes the tagged stores of the third kind. The window says so on such a watch.
- A line that writes a watched value is marked in the code view before the watch fires.
- In a recorded run, a watch needs no setup: it is a search of the trace, and it works backwards.
- The address check must cost nothing in a run without watches.

Breakpoints and watches are per user. They are stored in `ProjectRoot/.ps/local.json`, which is not committed
(`metadata.json` of `docs/AiWorkflowPlan.md` section 4.3 is). A watch on one object cannot outlive the run; the
other kinds are named by path and can.

## 9. Memory: stack and heap

The Memory mode changes the tree to memory regions: one stack per task, the heap, the program's built-in text (the
module string table), and "No owner".

**Stack.** Frames with the newest call on top. Each frame lists its slots: byte offset, name, value. A slot that
holds an address shows where it points: to a slot of a frame further down (a reference to a caller's local), or
into the heap.

**Heap.** Blocks in address order, wrapped into rows, colored by the struct that owns them; free space in grey, "no
owner" in red, blocks the walk cannot type in a color of their own. Below: the largest owners with their sizes,
total free space and the largest free run.

**The link between them.** Stack and heap are side by side. Selecting a slot that points into the heap outlines the
block and draws a line to it; a note says what the block is, how far into it the address lands, and which function
allocated it. Selecting a block shows who points to it.

On the VM all tasks share one heap; a block can be colored by the task that allocated it. On `optexe` each task has
an arena (`docs/MultithreadingPrototype.md`), and the heap view then has one map per arena. The first version
covers the VM only.

A heap of hundreds of megabytes needs zoom: at the far level blocks merge into colored density.

## 10. Time

- **Named events.** The timeline is a row of events, one lane per task: launch, a key press, a file change, a task
  start, an allocation that later has no owner. The runtime names the events it delivers itself; anything finer the
  program emits (the call for that is open, section 13).
- **Stepping** by event, by call (over, into, out) and by line. The toolbar holds both the event steps and the code
  steps without becoming a row of look-alike arrows; which set is shown first follows the selected item.
- **Going back.** With a recorded trace, every step has a backward twin, and history entries and "Go to previous
  change" jump to a sequence number through replay.
- **A running program.** The first version inspects a paused or replayed program. A window that updates while the
  program runs is later work.

## 11. What the VM and compiler need

| Need | For | Size |
| --- | --- | --- |
| A state snapshot in JSON: frames with typed locals, heap blocks, free runs, the ownership tree | Everything | Medium; the walk is new, the rest is formatting |
| The typed walk from roots through struct layouts and the known collection shapes | Tree, owners, "no owner" | Medium |
| The allocation table for debug runs | "Created by", untyped blocks | Small |
| Struct values that expand into fields in `variables` | Objects | Small once the walk exists |
| Step over, into and out | Stepping by call and line | Small |
| The range in which each local is live, in `IrLocalDebugSlot` | "Not set yet" | Small; a PSIR format change |
| Debug sessions for programs with tasks (they fault today) and one DAP thread per task | Tasks, stacks per task | Medium |
| A write log in the trace: address, old value, new value, instruction | Changes, history, inline values, backward watches | Large; the main cost of this plan |
| Watched address ranges in the store opcodes, carried through `HeapRealloc` | Watches on an object | Medium |
| Tags on the stores that assign a struct field | Watches on a struct's field | Medium; debug info only |
| Named events from the runtime | Timeline | Small per event source |
| A table of open host handles | Resources group | Not checked whether one exists |

Not checked: what `--debug-trace` records beyond instruction steps, how the VM places a struct value in a frame's
slots, and whether PrimeScroll itself runs under a VM debug session (it uses native UI host calls). The last one
decides whether the editor can be its own first test program.

All of it is for unoptimized runs.

## 12. Slices in order

1. **Snapshot v1.** `primevm` emits the state snapshot at a stop: frames with typed locals, heap blocks, free runs.
   Golden-file tests.
2. **Typed walk and allocation table.** The ownership tree, retained sizes, "no owner".
3. **Inspector window v1.** The tree in "State" with objects, fields value first, struct tables; a session started
   from PrimeScroll against `primevm --debug-dap`; line breakpoints and single steps. Read-only.
4. **Functions in the tree.** Code view with gutter breakpoints, locals under functions, live ranges, step over,
   into and out.
5. **Memory mode.** Stack and heap views and the link between them.
6. **Watches on locals and on struct fields** (the two breakpoint-set kinds), then on one object.
7. **Write log.** Changes view, history, inline values, backward stepping and backward watches.
8. **Named events and the timeline.**
9. **Tasks:** debug sessions with task opcodes, a stack per task.
10. **Later:** conditions, a live-updating window, `optexe` arenas, zoom for large heaps, comparing two moments
    side by side.

Slices 1 to 3 are useful on their own and do not depend on the write log.

## 13. Open decisions

- **How a struct names its label field** (section 5): a transform such as `[label]` on a field, or a convention.
- **Whether the window ever edits state.** Smalltalk does. Editing breaks replay and determinism, so the plan is
  read-only until the viewer has proved itself.
- **How a program emits a named event**, and whether that needs an effect.
- **Whether the snapshot extends DAP** (custom requests) or is a protocol of its own beside it.
- **The cost limit of the write log** in trace size and run time, and whether it is on by default in a debug run.
- **Where the inspector lives in PrimeScroll:** a window of its own, or a mode of the project window of
  `docs/AiWorkflowPlan.md` section 4.1.
