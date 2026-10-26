#!/usr/bin/env python3
"""Prints the Info.plist keys for the document types of an app (scripts/bundle_macos_app.sh puts them in the plist).

The input is a file_types.txt: one `name|extensions|optional type identifier` per line, extensions separated by
commas, `#` comment lines and blank lines skipped (see examples/apps/text_editor/file_types.txt). The output is the
`CFBundleDocumentTypes` array (an alternate handler for every type, the owner for a type the app exports, and a
catch-all entry for any other file that is never a default) and, for the
lines with a type identifier, a `UTExportedTypeDeclarations` array that declares them as source code.
"""

import sys
from xml.sax.saxutils import escape


def strings(values):
    return "".join(f"<string>{escape(v)}</string>" for v in values)


def main():
    if len(sys.argv) != 2:
        print("usage: document_types_plist.py file_types.txt", file=sys.stderr)
        return 64
    documents = []
    exported = []
    with open(sys.argv[1], encoding="utf-8") as handle:
        for raw in handle:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = [part.strip() for part in line.split("|")]
            if len(parts) < 2 or not parts[0] or not parts[1]:
                print(f"bad file type line: {line}", file=sys.stderr)
                return 65
            name = parts[0]
            extensions = [e.strip().lstrip(".").lower() for e in parts[1].split(",") if e.strip()]
            identifier = parts[2] if len(parts) > 2 and parts[2] else ""
            entry = (
                f"<dict><key>CFBundleTypeName</key><string>{escape(name)}</string>"
                f"<key>CFBundleTypeRole</key><string>Editor</string>"
                f"<key>LSHandlerRank</key><string>{'Owner' if identifier else 'Alternate'}</string>"
                f"<key>CFBundleTypeExtensions</key><array>{strings(extensions)}</array>"
            )
            if identifier:
                entry += f"<key>LSItemContentTypes</key><array>{strings([identifier])}</array>"
                exported.append(
                    f"<dict><key>UTTypeIdentifier</key><string>{escape(identifier)}</string>"
                    f"<key>UTTypeDescription</key><string>{escape(name)}</string>"
                    f"<key>UTTypeConformsTo</key><array>{strings(['public.source-code', 'public.plain-text'])}</array>"
                    f"<key>UTTypeTagSpecification</key><dict><key>public.filename-extension</key>"
                    f"<array>{strings(extensions)}</array></dict></dict>"
                )
            entry += "</dict>"
            documents.append(entry)
    # Every other file: dropped on the Dock icon, and offered in Open With. Handler rank None: the app is never
    # anybody's default for them by itself.
    documents.append(
        "<dict><key>CFBundleTypeName</key><string>Any file</string>"
        "<key>CFBundleTypeRole</key><string>Editor</string>"
        "<key>LSHandlerRank</key><string>None</string>"
        f"<key>LSItemContentTypes</key><array>{strings(['public.data'])}</array></dict>"
    )
    out = "<key>CFBundleDocumentTypes</key><array>" + "".join(documents) + "</array>"
    if exported:
        out += "<key>UTExportedTypeDeclarations</key><array>" + "".join(exported) + "</array>"
    print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
