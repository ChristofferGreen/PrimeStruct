# Web Apps: Plan

Classification: **design direction**. The open work is in `docs/todo.md` (Web apps and Native UI handlers lanes).

## Where this starts

`/std/net`, `/std/os`, `/std/http` (request parser with decoded query and form parameters, `Response` with `html` and
`redirect`, `Router` with `:name` path parameters, static files), `/std/http/server`, `/std/json` and `/std/html` (an
escaping HTML writer with the form vocabulary) exist and run on the VM and `--emit=optexe`
(docs/spec/stdlib-reference.md). Servers are single-threaded optexe binaries; containers are out of scope. The server
reads one connection at a time. One file cannot import both `/std/html` and `/std/json` yet: both export `Writer`
(TODO-5717). Repeated parameters give their first value and path parameters are not decoded (TODO-5718).

## Principles

1. **One handler model, not one widget tree.** Web pages and native windows do not share widgets: HTML and AppKit
   differ in layout, text and focus, and a common tree collapses to the lowest denominator (the `/std/ui` prototype,
   `stdlib/std/ui/ui.prime`, stopped at a login form). What they share is how events reach code: a handler is a
   closure that takes the event and the state it acts on, it is registered in a table, and a loop dispatches to it.
   `Router` already works this way; the native UI moves to it too.
2. **State comes in through a parameter, never through a capture.** A closure with a `[ref]` capture cannot be stored
   (docs/spec/value-lifecycle.md, Lambdas), so stored handlers receive their state as an argument, the way HTTP
   handlers receive the `Response`. Value captures are for configuration and per-handler counters.
3. **Server-rendered HTML first.** A page is HTML written by the server; a button is a `<button>` in a
   `<form method="post">`, and its click handler is a POST route that redirects back (post/redirect/get). This works
   with no JavaScript at all.
4. **Then fragments, still without user JavaScript.** The stdlib serves one small fixed script. Elements carry
   attributes that say which route to call and which element to replace; the route answers with an HTML fragment.
   A button then has a PrimeStruct handler and nothing else. Because the server is single-threaded, app state is
   plain struct fields with no locking.
5. **Escaping is the default.** The HTML writer escapes text and attribute values; raw HTML is a separately named
   call.
6. **Client-side PrimeStruct in the browser is not on this path.** The wasm backends have no `String` and reject
   `net_io`.

## Target shape

The spelling below needs TODO-5707 (`mut` parameters in `Function` types). A lambda or a named handler already
passes where a `Function` is expected (TODO-5669, done), and a string literal where a `StringView` is expected
(TODO-5700, done).

```prime
[struct]
Counter() {
  [i32 mut] clicks{0i32}
}

router.get("/", ([Counter mut] app, [Request] request, [Response mut] response) {
  response.html(counter_page(app.clicks))
})
router.post("/add", ([Counter mut] app, [Request] request, [Response mut] response) {
  app.clicks = app.clicks + 1i32
  response.redirect("/")
})
```

The native UI reads the same way: `menu.item("Save", "cmd+s", handler)` and `button.onClick(handler)`, with an
`App.run(state)` that dispatches on command id. Command ids stay in the C ABI; `waitEvent` stays as the low level, so
PrimeScroll can move one menu at a time.

## Order

1. Language: done (TODO-5669, TODO-5700).
2. HTML and requests: TODO-5701 (writer, `Response.html`, `redirect`; done), TODO-5702 (query and form parameters;
   done), TODO-5703 (path parameters; done), then the todo-list example TODO-5704.
3. Server: TODO-5705 (an idle connection does not stall the others).
4. Fragments: TODO-5706.
5. Native UI: TODO-5709 (handlers and `App.run`), TODO-5710 (PrimeScroll on handlers), TODO-5711 (retire the
   `/std/ui` prototype).
6. Later: TODO-5707, TODO-5708 (type aliases), TODO-5717 (both writers in one file), TODO-5718 (repeated and decoded
   parameters), sessions and cookies, keep-alive, concurrency once tasks schedule.
