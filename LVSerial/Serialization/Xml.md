# UI Creator XML v1

UI Creator XML is an independent, editable representation of the same entity
records used by `Serialization::Protobuf`. It is not LVGL Pro XML and does not
implement or depend on that specification. The normal project loader restores
widgets, variables, assets and keyboard models after decoding either format.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<uicreator format_version="1" kind="project" type="project">
  <object>
    <property name="activeScreen"><integer>0</integer></property>
    <property name="screens">
      <array>
        <object>
          <property name="name"><string>Main</string></property>
          <property name="obj"><array/></property>
        </object>
      </array>
    </property>
  </object>
</uicreator>
```

The root has exactly three required attributes: `format_version="1"`, `kind`
and `type`. `kind` is `widget`, `screen`, `variable`, `project`, `style`, `font`
or `asset`; `type` is a nonempty UTF-8 entity type, such as `project`, `screen`
or a widget name. The root contains exactly one typed value.

| Value | Representation |
| --- | --- |
| Object | `<object>` containing `<property name="key">` elements, each with exactly one typed value |
| Array | `<array>` containing typed values directly, in order |
| String | `<string>text</string>`; whitespace and Unicode are preserved |
| Signed integer | `<integer>-42</integer>`, within signed 64-bit range |
| Unsigned integer | `<unsigned>42</unsigned>`, within unsigned 64-bit range |
| Real | `<real>1.25</real>`, a finite double; its floating type and negative zero are preserved |
| Boolean | `<boolean>true</boolean>` or `<boolean>false</boolean>` |
| Null | `<null/>` |
| Binary | `<binary encoding="base64">AQID</binary>`, optionally with an unsigned 64-bit `subtype` attribute |

Objects, arrays, strings and binary values may be empty. Empty objects and arrays
retain their distinct types. Base64 applies only to actual binary fields; it
does not wrap a Protobuf record or hide the project hierarchy. Assets already
stored as base64 strings in project records remain ordinary string properties.
Binary data is owned after decoding; XML has no borrowed-asset mode.

The encoder orders object properties deterministically, indents by two spaces,
and uses XML entities for metacharacters and numeric references for tab, newline
and carriage return. Its strings and property names round-trip exactly. A
handwritten document follows XML 1.0 line-ending and attribute-whitespace rules:
literal CR/CRLF becomes LF in text and literal whitespace in attributes becomes
spaces. Numeric references preserve those characters exactly. The five standard
entities and decimal/hexadecimal character references are supported. Comments
are allowed between values and inside scalar text. CDATA is not part of v1.

The reader accepts an optional UTF-8 BOM, an optional XML 1.0 declaration at the
start, and ordinary XML whitespace/comments outside the root. It rejects DTDs,
external or unknown entities, processing instructions, namespaces, duplicate
properties/attributes, unknown tags or attributes, unsupported versions,
multiple roots/values, malformed base64, invalid UTF-8/XML characters, integer
overflow and non-finite/out-of-range real numbers. It never opens external files
or resolves entities.

`XmlLimits` defaults to 64 MiB input/output, 64 nested value levels, one million
XML elements and 64 MiB per text field. Root typed-value depth is zero; property
wrappers do not add value depth but do count toward the element limit. A caller
can lower all limits; `maxDepth` cannot exceed 256. Runtime entry points may
apply a lower file-size limit before invoking the codec. Limits apply to both
encoding and decoding, before output files are written by their callers.
