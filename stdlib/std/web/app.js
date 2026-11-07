// The fragment script of /std/web (docs/spec/stdlib-reference.md, /std/web), served at GET /_web/app.js. A form
// with data-ps-target="id" posts with fetch and the header X-PrimeStruct-Fragment: id. An answer with that header
// replaces the contents of the element with that id; any other answer reloads the page. Without this script the
// same forms post and redirect back (post/redirect/get). Budget: 5120 bytes.
(function () {
  "use strict";
  var header = "X-PrimeStruct-Fragment";
  var queue = Promise.resolve();

  // Posts `body` to `url` for fragment `target`, one request at a time, so swaps land in the order sent.
  function post(url, body, target) {
    queue = queue.then(function () {
      var headers = {};
      headers[header] = target;
      return fetch(url, {method: "POST", body: body, headers: headers, redirect: "manual"});
    }).then(function (answer) {
      if (answer.headers.get(header) !== target) {
        location.reload();
        return;
      }
      return answer.text().then(function (html) {
        var element = document.getElementById(target);
        if (element) {
          element.innerHTML = html;
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
})();
