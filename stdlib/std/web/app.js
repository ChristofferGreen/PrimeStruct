// The fragment script of /std/web (docs/spec/stdlib-reference.md, /std/web), served at GET /_web/app.js. A form
// with data-ps-target="id" posts with fetch and the header X-PrimeStruct-Fragment: id. An answer with that header
// replaces the contents of element id; any other answer reloads the page. A button with data-ps-on="mouseover ..."
// also posts those events to /_on/<data-ps-id>/<event>. Without this script the forms post and redirect back
// (post/redirect/get). Budget: 4096 bytes.
(function () {
  "use strict";
  var header = "X-PrimeStruct-Fragment";
  var queue = Promise.resolve();
  var hover = 0;
  var keys = {Enter: -1, Tab: -2, Escape: -3, Backspace: -4, Delete: -5, ArrowLeft: -6, ArrowRight: -7,
    ArrowUp: -8, ArrowDown: -9, Home: -10, End: -11, PageUp: -12, PageDown: -13};

  // Posts `body` to `url` for fragment `target`, one request at a time, so swaps land in the order sent. A swap
  // gives focus back to the control that had it.
  function post(url, body, target) {
    queue = queue.then(function () {
      var headers = {};
      headers[header] = target;
      return fetch(url, {method: "POST", body: body, headers: headers, redirect: "manual"});
    }).then(function (answer) {
      if (!target || answer.headers.get(header) !== target) {
        location.reload();
        return;
      }
      return answer.text().then(function (html) {
        var element = document.getElementById(target);
        var id = document.activeElement.getAttribute("data-ps-id");
        if (element) {
          element.innerHTML = html;
          var again = id && document.querySelector("[data-ps-id=\"" + id + "\"]");
          if (again) {
            again.focus();
          }
        }
      });
    }).catch(function () {
      location.reload();
    });
  }

  document.addEventListener("submit", function (event) {
    var form = event.target;
    var target = form.getAttribute("data-ps-target");
    if (target && form.method === "post") {
      event.preventDefault();
      post(form.action, new URLSearchParams(new FormData(form)), target);
    }
  });

  // The control around `node` that has a handler for `kind`, or null.
  function control(node, kind) {
    var found = node && node.closest ? node.closest("[data-ps-on]") : null;
    return found && (" " + found.getAttribute("data-ps-on") + " ").indexOf(" " + kind + " ") >= 0 ? found : null;
  }

  // Posts event `kind` of control `found` with the fields of its /std/ui/events struct.
  function send(found, kind, event, fields) {
    fields.modifiers = (event.shiftKey ? 1 : 0) + (event.ctrlKey ? 2 : 0) + (event.altKey ? 4 : 0) +
      (event.metaKey ? 8 : 0);
    post("/_on/" + found.getAttribute("data-ps-id") + "/" + kind, new URLSearchParams(fields),
      found.form && found.form.getAttribute("data-ps-target") || "");
  }

  // A PointerEvent: the button (-1 none, 0 primary, 1 secondary, 2 middle) and the position in the control.
  function pointer(found, kind, event, button) {
    var box = found.getBoundingClientRect();
    send(found, kind, event, {button: button, x: Math.round(event.clientX - box.left),
      y: Math.round(event.clientY - box.top)});
  }

  // The pointer must rest on a control for 100 ms to send one mouseover; leaving sooner sends nothing.
  document.addEventListener("mouseover", function (event) {
    var found = control(event.target, "mouseover");
    if (found && !found.contains(event.relatedTarget)) {
      clearTimeout(hover);
      hover = setTimeout(function () { pointer(found, "mouseover", event, -1); }, 100);
    }
  });
  document.addEventListener("mouseout", function (event) {
    var found = control(event.target, "mouseover");
    if (found && !found.contains(event.relatedTarget)) {
      clearTimeout(hover);
    }
  });
  ["mousedown", "mouseup"].forEach(function (kind) {
    document.addEventListener(kind, function (event) {
      var found = control(event.target, kind);
      var button = [0, 2, 1][event.button];
      if (found && button !== undefined) {
        pointer(found, kind, event, button);
      }
    });
  });

  // A press focuses a control with key handlers, which some browsers do not do for buttons.
  document.addEventListener("mousedown", function (event) {
    var found = control(event.target, "keydown") || control(event.target, "keyup");
    if (found) {
      setTimeout(function () { found.focus(); });
    }
  });

  // A KeyEvent, for the focused control only: the code point of the key, or a named key below 0.
  ["keydown", "keyup"].forEach(function (kind) {
    document.addEventListener(kind, function (event) {
      var found = control(document.activeElement, kind);
      var key = Array.from(event.key).length === 1 ? event.key.codePointAt(0) : keys[event.key];
      if (found && key !== undefined) {
        send(found, kind, event, {key: key, repeat: event.repeat ? 1 : 0});
      }
    });
  });
})();
