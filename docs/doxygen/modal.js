(function () {
    "use strict";

    function createModal() {
        var overlay = document.createElement("div");
        overlay.className = "svg-modal-overlay";
        overlay.setAttribute("role", "dialog");
        overlay.setAttribute("aria-modal", "true");
        overlay.setAttribute("aria-hidden", "true");

        overlay.innerHTML =
            '<div class="svg-modal-dialog">' +
            '  <div class="svg-modal-header">' +
            '    <span class="svg-modal-title"></span>' +
            '    <button type="button" class="svg-modal-close" aria-label="Cerrar diagrama">&times;</button>' +
            "  </div>" +
            '  <div class="svg-modal-body">' +
            '    <img src="" alt="" />' +
            "  </div>" +
            "</div>";

        document.body.appendChild(overlay);

        var titleEl = overlay.querySelector(".svg-modal-title");
        var imgEl = overlay.querySelector(".svg-modal-body img");
        var closeBtn = overlay.querySelector(".svg-modal-close");

        function closeModal() {
            overlay.classList.remove("is-open");
            overlay.setAttribute("aria-hidden", "true");
            imgEl.removeAttribute("src");
            document.body.style.overflow = "";
        }

        function openModal(src, title) {
            imgEl.src = src;
            imgEl.alt = title || "Diagrama SVG";
            titleEl.textContent = title || "Diagrama SVG";
            overlay.classList.add("is-open");
            overlay.setAttribute("aria-hidden", "false");
            document.body.style.overflow = "hidden";
            closeBtn.focus();
        }

        closeBtn.addEventListener("click", closeModal);
        overlay.addEventListener("click", function (ev) {
            if (ev.target === overlay) {
                closeModal();
            }
        });
        document.addEventListener("keydown", function (ev) {
            if (ev.key === "Escape" && overlay.classList.contains("is-open")) {
                closeModal();
            }
        });

        return { open: openModal };
    }

    function initSvgThumbnails() {
        var modal = createModal();
        var candidates = document.querySelectorAll(
            '.contents .image img[src$=".svg"], ' +
            '.contents .image object[type="image/svg+xml"][data$=".svg"], ' +
            '.contents p > img[src$=".svg"]'
        );

        candidates.forEach(function (el) {
            var isObject = el.tagName.toLowerCase() === "object";
            var src = el.getAttribute(isObject ? "data" : "src") || "";
            if (!src || src.indexOf("doxygen.svg") !== -1) {
                return;
            }

            var parent = el.parentElement;
            var captionText = "";
            if (parent && parent.classList.contains("image")) {
                var cap = parent.querySelector(".caption");
                if (cap) captionText = cap.textContent.trim();
            }
            var title = captionText || el.getAttribute("alt") || "Diagrama SVG";

            var box = document.createElement("div");
            box.className = "svg-thumb-box";
            box.setAttribute("role", "button");
            box.setAttribute("tabindex", "0");
            box.setAttribute("aria-label", "Ampliar diagrama: " + title);

            var imgEl;
            if (isObject) {
                imgEl = document.createElement("img");
                imgEl.src = src;
                imgEl.alt = title;
                parent.insertBefore(box, el);
                parent.removeChild(el);
            } else {
                imgEl = el;
                parent.insertBefore(box, el);
            }
            box.appendChild(imgEl);

            var hint = document.createElement("span");
            hint.className = "svg-thumb-hint";
            hint.textContent = "Clic para ampliar";
            box.appendChild(hint);

            function handleOpen() {
                modal.open(src, title);
            }

            box.addEventListener("click", handleOpen);
            box.addEventListener("keydown", function (ev) {
                if (ev.key === "Enter" || ev.key === " ") {
                    ev.preventDefault();
                    handleOpen();
                }
            });
        });
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", initSvgThumbnails);
    } else {
        initSvgThumbnails();
    }
})();
