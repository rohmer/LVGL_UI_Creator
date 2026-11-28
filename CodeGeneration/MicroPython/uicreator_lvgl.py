"""LVGL UI Creator runtime for lvgl/lv_binding_micropython (LVGL 9.3).

Create and retain the board's display/input drivers before loading a UI. All
methods must run on the LVGL/MicroPython owner thread. This module does not
select pins, initialize hardware, or create another event loop.
"""
try:
    import ujson as json
except ImportError:
    import json
import lvgl as lv
from array import array


_KINDS = (("text", "label"), ("button", "button"), ("cb", "checkbox"),
          ("arc", "arc"), ("bar", "bar"), ("btnm", "buttonmatrix"),
          ("cal", "calendar"), ("canvas", "canvas"), ("img", "image"),
          ("dropdown", "dropdown"), ("roller", "roller"), ("slider", "slider"),
          ("spinbox", "spinbox"), ("switch", "switch"), ("textarea", "textarea"),
          ("led", "led"), ("line", "line"), ("spinner", "spinner"),
          ("keyboard", "keyboard"), ("table", "table"), ("chart", "chart"),
          ("tabview", "tabview"), ("tileview", "tileview"), ("window", "win"),
          ("messagebox", "msgbox"), ("imagebutton", "imagebutton"),
          ("scale", "scale"), ("qrcode", "qrcode"), ("barcode", "barcode"), ("page", "obj"))
_COLORS = ("bg_color", "bg_grad_color", "border_color", "outline_color",
           "shadow_color", "image_recolor", "text_color", "line_color",
           "arc_color", "bg_image_recolor")
_STATES = {"checked": "CHECKED", "disabled": "DISABLED", "state_user_1": "USER_1",
           "state_user_2": "USER_2", "state_user_3": "USER_3", "state_user_4": "USER_4"}
_INTEGER_RANGES = {"int8_t": (-128, 127), "uint8_t": (0, 255), "int16_t": (-32768, 32767),
                   "uint16_t": (0, 65535), "int32_t": (-(1 << 31), (1 << 31) - 1), "uint32_t": (0, (1 << 32) - 1)}
_INF = float("inf")
# LVGL's keyboard layouts are process-global per mode. Its C globals are not
# MicroPython GC roots, so retain the most recently registered layout for each
# mode. Individual UIs also retain snapshots used by their live keyboards.
_KEYBOARD_MAP_ROOTS = {}


def _call(obj, method, *args):
    function = getattr(obj, method, None)
    if function is None:
        raise RuntimeError("MicroPython firmware is missing LVGL API: " + method)
    return function(*args)


def _text(value):
    if value is True:
        return "true"
    if value is False:
        return "false"
    return str(value)


def _coord(value):
    if not isinstance(value, dict):
        return value
    kind = value.get("coordinateKind")
    if kind == "percent" or "percent" in value:
        return lv.pct(value.get("value", value.get("percent")))
    if kind == "content" or value.get("content"):
        return lv.SIZE_CONTENT
    if kind == "fraction" or "gridFr" in value:
        return lv.grid_fr(value.get("value", value.get("gridFr")))
    if kind == "gridContent" or value.get("gridContent"):
        return lv.GRID_CONTENT
    if kind == "maximum":
        return getattr(lv, "COORD_MAX", 536870911)
    if kind == "minimum":
        return getattr(lv, "COORD_MIN", -536870911)
    raise ValueError("Unsupported coordinate: " + str(value))


def _flag(obj, name, enabled):
    if name in _STATES:
        state = getattr(lv.STATE, _STATES[name])
        _call(obj, "add_state" if enabled else "remove_state", state)
        return
    flag = getattr(lv.obj.FLAG, name.upper(), None)
    if flag is None:
        if enabled:
            raise ValueError("Object flag is unavailable in LVGL 9.3: " + name)
        return
    _call(obj, "add_flag" if enabled else "remove_flag", flag)


class UI:
    """Owns screens, callback references, image bytes and variable bindings.

    load() is transactional: an exception deletes every partially created
    screen. deinit() cancels animation timers and releases this UI's screens.
    No global LVGL deinitialization or display ownership is taken.
    """

    def __init__(self, project_path="ui.json", asset_root="assets"):
        self.project_path = project_path
        self.asset_root = asset_root.rstrip("/")
        self.screens = []
        self.objects = {}
        self.names = {}
        self.variables = {}
        self._definitions = {}
        self._bindings = {}
        self._histories = {}
        self._transitions = []
        self._callbacks = []
        self._resources = []
        self._images = {}
        self._image_data = {}
        self._timers = []
        self._keyboards = []
        self._textareas = []
        self._records = {}
        self._updating = False
        self._dispatching = False
        self._previous_screen = None
        self._advanced = None
        self.active_screen = -1

    def load(self):
        if self.screens:
            raise RuntimeError("UI is already loaded; call deinit() first")
        if lv.display_get_default() is None:
            raise RuntimeError("Initialize your LVGL display and input drivers before loading this UI")
        if isinstance(self.project_path, dict):
            document = self.project_path
        else:
            with open(self.project_path, "r") as source:
                document = json.load(source)
        keyboard = document.get("advancedKeyboard")
        if "advancedKeyboard" in document:
            if not isinstance(keyboard, dict):
                raise ValueError("Project keyboard settings must be an object")
            if type(keyboard.get("enabled", False)) is not bool:
                raise ValueError("Project keyboard enabled must be a boolean")
            if keyboard.get("enabled", False):
                raise ValueError("Widget is not supported by the upstream MicroPython binding: advancedKeyboard (project setting)")
        definitions = document.get("screens")
        if definitions is None:
            definitions = [{"canvas": document.get("canvas", {}), "obj": document.get("obj", [])}]
        if not definitions:
            raise ValueError("A UI needs at least one screen")
        self._previous_screen = lv.screen_active()
        try:
            for index, definition in enumerate(definitions):
                canvas = definition.get("canvas", {})
                screen = lv.obj(None)
                self.screens.append(screen)
                self._transitions.append(definition.get("transition", {}))
                self._base(screen, canvas.get("base", {}))
                if "width" in canvas:
                    screen.set_width(_coord(canvas["width"]))
                if "height" in canvas:
                    screen.set_height(_coord(canvas["height"]))
                self.names[definition.get("name", str(index))] = index
                pending = list(definition.get("obj", []))
                if "advancedKeyboard" in document:
                    # Explicitly disabling the project service also supersedes
                    # legacy authored keyboards, matching native exports.
                    pending = [record for record in pending if "advancedKeyboard" not in record]
                while pending:
                    deferred = []
                    for record in pending:
                        parent_id = record.get("par", -1)
                        if parent_id >= 0 and (index, parent_id) not in self.objects:
                            deferred.append(record)
                            continue
                        parent = screen if parent_id < 0 else self.objects[index, parent_id]
                        for child in record.get("hostPath", []):
                            parent = parent.get_child(child)
                            if parent is None:
                                raise ValueError("Object hostPath refers to a missing managed child")
                        obj = self._create(record, parent, index)
                        oid = record.get("oid", -1)
                        if oid < 0 or (index, oid) in self.objects:
                            raise ValueError("Missing or duplicate object ID")
                        self.objects[index, oid] = obj
                        self._records[index, oid] = record
                    if len(deferred) == len(pending):
                        raise ValueError("Missing parent or cyclic object hierarchy")
                    pending = deferred
            self._load_variables(document.get("variables", []))
            for (screen, oid), record in self._records.items():
                self._bind_events(self.objects[screen, oid], screen, oid, record.get("base", {}).get("events", []))
            self._link_keyboards()
            self.show_screen(document.get("activeScreen", 0))
            # Authored records are no longer needed after widget construction.
            self._records.clear()
            return self
        except Exception:
            self.deinit()
            raise

    def get_object(self, screen, oid):
        if isinstance(screen, str):
            screen = self.names[screen]
        return self.objects[screen, oid]

    def show_screen(self, screen):
        if isinstance(screen, str):
            screen = self.names[screen]
        if not isinstance(screen, int) or screen < 0 or screen >= len(self.screens):
            raise ValueError("Screen index is outside this UI")
        transition = self._transitions[screen]
        kind = transition.get("type", "none")
        duration = transition.get("duration", 0)
        if kind != "none" and duration:
            animations = {"fade": "FADE_IN", "slide_left": "MOVE_LEFT", "slide_right": "MOVE_RIGHT",
                          "slide_up": "MOVE_TOP", "slide_down": "MOVE_BOTTOM"}
            lv.screen_load_anim(self.screens[screen], getattr(lv.SCR_LOAD_ANIM, animations[kind]), duration, 0, False)
        else:
            lv.screen_load(self.screens[screen])
        self.active_screen = screen
        return self.screens[screen]

    def deinit(self):
        for timer in self._timers:
            timer.delete()
        self._timers = []
        if self.screens:
            # Loading the already active screen does not cancel a pending load
            # in LVGL 9.3. A temporary screen completes that interruption path.
            if self._previous_screen is not None and lv.screen_active() == self._previous_screen and any(
                    entry.get("type", "none") != "none" and entry.get("duration", 0) for entry in self._transitions):
                guard = lv.obj(None)
                lv.screen_load(guard)
                lv.screen_load(self._previous_screen)
                guard.delete()
            if lv.screen_active() in self.screens and self._previous_screen is not None:
                lv.screen_load(self._previous_screen)
            for screen in self.screens:
                screen.delete()
        self.screens = []
        self.objects.clear()
        self.names.clear()
        self.variables.clear()
        self._definitions.clear()
        self._bindings.clear()
        self._histories.clear()
        self._transitions = []
        self._callbacks = []
        self._resources = []
        self._images.clear()
        self._image_data.clear()
        self._keyboards = []
        self._textareas = []
        self._records.clear()
        self._advanced = None
        self.active_screen = -1

    def _on(self, obj, callback, event):
        self._callbacks.append(callback)
        obj.add_event_cb(callback, event, None)

    def _font(self, value):
        if value in (None, "default"):
            return lv.font_get_default()
        if isinstance(value, dict):
            name = value.get("name", value.get("font", ""))
        else:
            name = str(value)
        if name.startswith("lv_font_"):
            name = name[3:]
        if not name.startswith("font_"):
            name = "font_" + name
        result = getattr(lv, name, None)
        if result is None:
            raise ValueError("Font is not enabled in the MicroPython firmware: " + name)
        return result

    def _image(self, value):
        if value is None or value == {}:
            return None
        if isinstance(value, str):
            return value
        if value.get("kind") == "text":
            return value.get("source", "")
        if "sym" in value and "mpyAsset" not in value:
            return value["sym"]
        name = value.get("mpyAsset")
        if not name:
            raise ValueError("Image was not normalized by the MicroPython exporter")
        if "/" in name or "\\" in name or name in (".", ".."):
            raise ValueError("Invalid image asset filename")
        key = (name, value["width"], value["height"], value["stride"], value["colorFormat"], value.get("flags", 0))
        if key in self._images:
            return self._images[key]
        data = self._image_data.get(name)
        if data is None:
            with open(self.asset_root + "/" + name, "rb") as source:
                data = source.read()
            self._image_data[name] = data
            self._resources.append(data)
        descriptor = lv.image_dsc_t({"header": {"magic": 0x19,
            "cf": value["colorFormat"], "flags": value.get("flags", 0),
            "w": value["width"], "h": value["height"], "stride": value["stride"]},
            "data_size": len(data), "data": data})
        self._images[key] = descriptor
        return descriptor

    def _style(self, obj, values, selector=0):
        for name, value in values.items():
            if name in _COLORS or name.endswith("_color") or name.endswith("_recolor") or name == "recolor":
                value = lv.color_hex(value & 0xffffff)
            elif name == "text_font":
                value = self._font(value)
            elif name in ("bg_image_src", "arc_image_src", "bitmap_mask_src"):
                setter = getattr(obj, "set_style_" + name, None)
                if setter is None:
                    raise RuntimeError("MicroPython firmware is missing LVGL style API: " + name)
                self._set_image_source(value, lambda source, apply=setter, part=selector: apply(source, part))
                continue
            elif name in ("grid_column_dsc_array", "grid_row_dsc_array"):
                backing = array("i", [_coord(track) for track in value] + [lv.GRID_TEMPLATE_LAST])
                # Passing a list (or array directly) makes this binding allocate
                # a temporary C array. A retained memoryview passes our buffer.
                value = memoryview(backing)
                self._resources.extend((backing, value))
            elif name == "bg_grad":
                self._gradient(obj, value, selector)
                continue
            else:
                value = _coord(value)
            _call(obj, "set_style_" + name, value, selector)

    def _gradient(self, obj, value, selector):
        stops = value.get("stops", [])
        if value.get("points") or value.get("direction", 0) > 2 or len(stops) > 2:
            raise ValueError("This MicroPython export supports horizontal/vertical two-stop gradients")
        obj.set_style_bg_grad_dir(value.get("direction", 0), selector)
        if stops:
            obj.set_style_bg_color(lv.color_hex(stops[0].get("color", 0)), selector)
            obj.set_style_bg_main_stop(stops[0].get("fraction", 0), selector)
            obj.set_style_bg_main_opa(stops[0].get("opacity", 255), selector)
        if len(stops) > 1:
            obj.set_style_bg_grad_color(lv.color_hex(stops[1].get("color", 0)), selector)
            obj.set_style_bg_grad_stop(stops[1].get("fraction", 255), selector)
            obj.set_style_bg_grad_opa(stops[1].get("opacity", 255), selector)

    def _legacy_style(self, obj, style, selector=0):
        body = style.get("body", {})
        text = style.get("text", {})
        pairs = ((body, "main_color", "bg_color"), (body, "gradColor", "bg_grad_color"),
                 (body, "opa", "bg_opa"), (body, "radius", "radius"),
                 (text, "color", "text_color"), (text, "opa", "text_opa"),
                 (text, "letter_space", "text_letter_space"), (text, "line_space", "text_line_space"))
        values = {}
        for source, key, name in pairs:
            if key in source:
                values[name] = source[key]
        if "gradColor" in body:
            values["bg_grad_dir"] = 2
        for section, prefix, fields in (("border", "border_", ("width", "color", "opa")),
                ("shadow", "shadow_", ("width", "color")),
                ("padding", "pad_", ("left", "right", "top", "bottom"))):
            for key in fields:
                if key in body.get(section, {}):
                    values[prefix + key] = body[section][key]
        if "inner" in body.get("padding", {}):
            values["pad_row"] = values["pad_column"] = body["padding"]["inner"]
        if "font" in text:
            values["text_font"] = text["font"]
        for section, fields in (("line", (("color", "line_color"), ("width", "line_width"),
                                        ("opa", "line_opa"), ("rounded", "line_rounded"))),
                                ("image", (("color", "image_recolor"), ("intense", "image_recolor_opa"),
                                           ("opa", "image_opa")))):
            for source, target in fields:
                if source in style.get(section, {}):
                    values[target] = style[section][source]
        self._style(obj, values, selector)

    def _base(self, obj, base):
        if "nativeStyles" in base:
            for style in base["nativeStyles"]:
                self._style(obj, style.get("values", {}), style.get("selector", 0))
        else:
            self._legacy_style(obj, base.get("style", {}))
        for key, value in base.get("coords", {}).items():
            if key in ("x", "y", "width", "height"):
                _call(obj, "set_" + key, _coord(value))
        for source, name in (("click", "clickable"), ("hidden", "hidden"), ("parevent", "event_bubble"),
                             ("checked", "checked"), ("disabled", "disabled")):
            if source in base:
                _flag(obj, name, base[source])
        for name, value in base.get("flags", {}).items():
            _flag(obj, name, value)
        if base.get("opascaleen"):
            obj.set_style_opa(base.get("opascale", 255), 0)
        scroll = base.get("scroll", {})
        for source, name in (("enabled", "scrollable"), ("elastic", "scroll_elastic"),
                             ("momentum", "scroll_momentum"), ("chainHorizontal", "scroll_chain_hor"),
                             ("chainVertical", "scroll_chain_ver")):
            if source in scroll:
                _flag(obj, name, scroll[source])
        for source, method in (("direction", "set_scroll_dir"), ("scrollbar", "set_scrollbar_mode"),
                               ("snapX", "set_scroll_snap_x"), ("snapY", "set_scroll_snap_y")):
            if source in scroll:
                _call(obj, method, scroll[source])
        if "x" in scroll or "y" in scroll:
            obj.scroll_to(scroll.get("x", 0), scroll.get("y", 0), False)
        if "bootstrapAction" in base:
            action = base["bootstrapAction"]
            if action not in ("dismiss", "collapse"):
                raise ValueError("Unknown Bootstrap action: " + action)
            def bootstrap(event, target=obj, operation=action):
                parent = target.get_parent()
                if parent is not None:
                    if operation == "dismiss":
                        _flag(parent, "hidden", True)
                    else:
                        body = parent.get_child(1)
                        if body is not None:
                            _flag(body, "hidden", not body.has_flag(lv.obj.FLAG.HIDDEN))
            self._on(obj, bootstrap, lv.EVENT.CLICKED)

    def _children(self, records, parent, screen, message=None):
        for record in records:
            cls = record.get("contentClass")
            if message is not None and cls in ("messageboxFooterButton", "messageboxHeaderButton"):
                if cls == "messageboxFooterButton":
                    obj = message.add_footer_button("")
                else:
                    obj = message.add_header_button(None)
                    if record.get("close"):
                        def hide(event, target=message):
                            _flag(target, "hidden", True)
                        self._on(obj, hide, lv.EVENT.CLICKED)
                obj.clean()
                self._base(obj, record.get("base", {}))
                self._children(record.get("contentChildren", []), obj, screen)
            else:
                self._create(record, parent, screen)

    def _create(self, record, parent, screen):
        for unsupported in ("advancedKeyboard", "textBox", "layerView"):
            if unsupported in record:
                raise ValueError("Widget is not supported by the upstream MicroPython binding: " + unsupported)
        for kind in ("radioGroup", "checkboxButton", "labeledCheckbox", "datePicker", "menu"):
            if kind in record:
                if self._advanced is None:
                    from uicreator_advanced import Controls
                    self._advanced = Controls(self)
                obj = self._advanced.create(kind, parent, record[kind])
                self._base(obj, record.get("base", {}))
                self._children(record.get("contentChildren", []), obj, screen)
                if "hostIndex" in record:
                    obj.move_to_index(record["hostIndex"])
                return obj
        kind, cls = "object", "obj"
        for key, native in _KINDS:
            if key in record:
                kind, cls = key, native
                break
        constructor = getattr(lv, cls, None)
        if constructor is None:
            raise RuntimeError("Widget is not enabled in MicroPython firmware: " + cls)
        if kind == "keyboard":
            settings = record[kind]
            mode = self._keyboard_mode(settings.get("mode", 0))
            if 4 <= mode <= 7 and mode not in _KEYBOARD_MAP_ROOTS and not settings.get("buttons", {}).get("btnMap"):
                raise ValueError("Keyboard mode USER requires a custom button map")
        obj = constructor(parent)
        self._configure(obj, kind, record.get(kind, {}), record, screen)
        self._base(obj, record.get("base", {}))
        self._children(record.get("contentChildren", []), obj, screen)
        if "hostIndex" in record:
            obj.move_to_index(record["hostIndex"])
        return obj

    def get_advanced_part(self, obj, name):
        if self._advanced is None:
            raise ValueError("This UI has no advanced controls")
        return self._advanced.part(obj, name)

    def get_advanced_value(self, obj):
        if self._advanced is None:
            raise ValueError("This UI has no advanced controls")
        return self._advanced.get(obj)

    def set_advanced_value(self, obj, value):
        if self._advanced is None:
            raise ValueError("This UI has no advanced controls")
        self._advanced.set(obj, value)

    def open_date_picker(self, obj):
        if self._advanced is None:
            raise ValueError("This UI has no advanced controls")
        self._advanced.open_date(obj)

    def close_date_picker(self, obj):
        if self._advanced is None:
            raise ValueError("This UI has no advanced controls")
        self._advanced.close_date(obj)

    def _keyboard_mode(self, value):
        if not isinstance(value, int) or isinstance(value, bool):
            raise ValueError("Keyboard mode must be an integer supported by the MicroPython firmware")
        if 0 <= value <= 7:
            return value
        modes = getattr(lv.keyboard, "MODE", None)
        if value == 8 and modes is not None and getattr(modes, "TEXT_ARABIC", None) == 8:
            return value
        raise ValueError("Keyboard mode is unavailable in this MicroPython firmware")

    def _configure(self, obj, kind, data, record, screen):
        native = "nativeStyles" in record.get("base", {})
        if kind == "text":
            obj.set_text(data)
            if "longmode" in record:
                modes = (0, 0, 1, 2, 3, 4)
                obj.set_long_mode(modes[record["longmode"]])
                if record["longmode"] == 0:
                    obj.set_size(lv.SIZE_CONTENT, lv.SIZE_CONTENT)
            if not native and "textalign" in record:
                obj.set_style_text_align(record["textalign"], 0)
            if not native and "drawback" in record:
                obj.set_style_bg_opa(255 if record["drawback"] else 0, 0)
        elif kind == "button":
            if "text" in data:
                caption = lv.label(obj)
                caption.set_text(data["text"])
                caption.center()
            if "toggle" in data:
                _flag(obj, "checkable", data["toggle"])
            layout = data.get("layout", 0)
            if layout:
                column = 2 <= layout <= 4
                flow = lv.FLEX_FLOW.COLUMN if column else lv.FLEX_FLOW.ROW_WRAP if layout >= 8 else lv.FLEX_FLOW.ROW
                align = lv.FLEX_ALIGN.CENTER if layout in (1, 3, 6) else lv.FLEX_ALIGN.END if layout in (4, 7) else lv.FLEX_ALIGN.START
                obj.set_flex_flow(flow)
                obj.set_flex_align(lv.FLEX_ALIGN.CENTER if layout == 1 else lv.FLEX_ALIGN.START, align, lv.FLEX_ALIGN.START)
            for side, fit in data.get("fit", {}).items():
                if fit:
                    _call(obj, "set_width" if side in ("left", "right") else "set_height", lv.SIZE_CONTENT if fit == 1 else lv.pct(100))
            if not native:
                for key, state in (("rel", 0), ("pr", lv.STATE.PRESSED), ("ina", lv.STATE.DISABLED),
                                   ("tglRel", lv.STATE.CHECKED), ("tglPr", lv.STATE.CHECKED | lv.STATE.PRESSED)):
                    self._legacy_style(obj, data.get("styles", {}).get(key, {}), state)
        elif kind == "cb":
            obj.set_text(data.get("text", ""))
            if "checked" in data:
                _flag(obj, "checked", data["checked"])
        elif kind in ("bar", "slider", "arc"):
            bounds = data.get("range", data)
            obj.set_range(bounds.get("min", 0), bounds.get("max", 100))
            obj.set_mode(data.get("mode", 1 if data.get("sym") else 0))
            if "orientation" in data:
                _call(obj, "set_orientation", data["orientation"])
            if kind == "arc":
                for key, method in (("rotation", "set_rotation"), ("knobOffset", "set_knob_offset"), ("changeRate", "set_change_rate")):
                    if key in data:
                        _call(obj, method, data[key])
                if "angle" in data:
                    obj.set_bg_angles(data["angle"].get("start", 0), data["angle"].get("end", 360))
                if "indicatorStart" in data and "indicatorEnd" in data:
                    obj.set_angles(data["indicatorStart"], data["indicatorEnd"])
                if data.get("value", -32768) != -32768:
                    obj.set_value(data["value"])
                if not native:
                    for source, key in (("line.rounded", "arc_rounded"), ("line.width", "arc_width"),
                                        ("line.color", "arc_color"), ("line.opa", "arc_opa")):
                        if source in data.get("style", {}):
                            self._style(obj, {key: data["style"][source]})
            else:
                obj.set_value(data.get("value", bounds.get("min", 0)), False)
                if kind == "bar" and data.get("mode") == 2:
                    obj.set_start_value(data.get("startValue", bounds.get("min", 0)), False)
                if kind == "slider" and data.get("mode") == 2:
                    obj.set_left_value(data.get("leftValue", bounds.get("min", 0)), False)
                if kind == "bar" and not native:
                    if "animT" in data:
                        obj.set_style_anim_duration(data["animT"], 0)
                    self._legacy_style(obj, data.get("styleBG", {}), lv.PART.MAIN)
                    self._legacy_style(obj, data.get("styleInd", {}), lv.PART.INDICATOR)
        elif kind == "dropdown":
            obj.set_options(data.get("options", ""))
            obj.set_selected(data.get("selected", 0))
            for key, method in (("direction", "set_dir"), ("text", "set_text"), ("symbol", "set_symbol"), ("highlight", "set_selected_highlight")):
                if key in data:
                    _call(obj, method, data[key])
        elif kind == "roller":
            obj.set_options(data.get("options", ""), data.get("mode", 0))
            obj.set_visible_row_count(data.get("visibleRows", 3))
            obj.set_selected(data.get("selected", 0), False)
        elif kind == "spinbox":
            obj.set_range(data.get("min", -99999), data.get("max", 99999))
            obj.set_digit_format(data.get("digits", 5), data.get("decimalPlaces", 0))
            obj.set_step(data.get("step", 1))
            obj.set_value(data.get("value", 0))
            obj.set_rollover(data.get("rollover", False))
            if "stepDirection" in data:
                obj.set_digit_step_direction(data["stepDirection"])
        elif kind == "switch":
            if "checked" in data:
                _flag(obj, "checked", data["checked"])
        elif kind == "textarea":
            for key, method in (("accepted", "set_accepted_chars"), ("maxLength", "set_max_length"),
                ("oneLine", "set_one_line"), ("password", "set_password_mode"),
                ("passwordBullet", "set_password_bullet"), ("passwordShowTime", "set_password_show_time"),
                ("placeholder", "set_placeholder_text"),
                ("cursorClick", "set_cursor_click_pos"), ("selection", "set_text_selection")):
                if key in data:
                    _call(obj, method, data[key])
            mode = data.get("inputMode", "alphanumeric")
            self._textarea_input_mode(obj, mode)
            if "text" in data:
                obj.set_text(data["text"])
            if "cursor" in data:
                obj.set_cursor_pos(data["cursor"])
            self._textareas.append((screen, record.get("oid", -1), obj, mode))
        elif kind == "led":
            obj.set_color(lv.color_hex(data.get("color", 0x3399ff)))
            obj.set_brightness(data.get("brightness", 255))
        elif kind == "line":
            records = data.get("points", [])
            points = lv.point_precise_t(len(records))
            for index, point in enumerate(records):
                points[index].x = point[0]
                points[index].y = point[1]
            self._resources.append(points)
            obj.set_points(points, len(records))
            obj.set_y_invert(data.get("invertY", False))
        elif kind == "spinner":
            obj.set_anim_params(data.get("duration", 1000), data.get("sweep", 60))
        elif kind == "btnm":
            self._button_map(obj, data)
        elif kind == "keyboard":
            # A default keyboard can borrow a previous UI's global custom map.
            # Keep that map alive even if another UI replaces its global slot.
            self._resources.extend(tuple(_KEYBOARD_MAP_ROOTS.values()))
            mode = self._keyboard_mode(data.get("mode", 0))
            if "buttons" in data:
                buttons = data["buttons"]
                labels = list(buttons.get("btnMap", []))
                if labels:
                    if labels[-1] != "":
                        labels.append("")
                    controls = list(buttons.get("nativeCtrl", []))
                    if not controls:
                        controls = [1] * sum(1 for label in labels if label not in ("", "\n"))
                    strings = self._string_pointers(labels)
                    backing = array("i", controls)
                    view = memoryview(backing)
                    control_pointer = lv.C_Pointer.__cast__(view)
                    roots = (labels, strings, backing, view, control_pointer)
                    self._resources.append(roots)
                    _KEYBOARD_MAP_ROOTS[mode] = roots
                    obj.set_map(mode, strings, control_pointer)
            # USER modes have null native maps until set_map supplies them.
            # Installing the map first avoids dereferencing those null globals.
            obj.set_mode(mode)
            obj.set_popovers(data.get("popovers", False))
            self._keyboards.append((screen, obj, data))
        elif kind == "cal":
            self._calendar(obj, data, not native)
        elif kind == "img":
            self._configure_image(obj, data)
        elif kind == "canvas":
            self._canvas(obj, data)
        elif kind == "table":
            self._table(obj, data)
        elif kind == "chart":
            self._chart(obj, data)
        elif kind in ("tabview", "tileview"):
            if kind == "tabview":
                obj.set_tab_bar_position(data.get("position", lv.DIR.TOP))
                obj.set_tab_bar_size(data.get("barSize", 40))
                self._base(obj.get_tab_bar(), data.get("barBase", {}))
                self._base(obj.get_content(), data.get("contentBase", {}))
                for panel in data.get("tabs", []):
                    host = obj.add_tab(panel.get("name", "Tab"))
                    self._base(host, panel.get("base", {}))
                    self._children(panel.get("children", []), host, screen)
                if data.get("tabs"):
                    obj.set_active(data.get("active", 0), False)
            else:
                panels = []
                for panel in data.get("tiles", []):
                    host = obj.add_tile(panel.get("column", 0), panel.get("row", 0), panel.get("directions", lv.DIR.ALL))
                    panels.append(host)
                    self._base(host, panel.get("base", {}))
                    self._children(panel.get("children", []), host, screen)
                if panels:
                    obj.set_tile(panels[data.get("active", 0)], False)
        elif kind == "window":
            obj.get_header().set_height(data.get("headerHeight", 40))
            if "title" in data:
                obj.add_title(data["title"])
            self._base(obj.get_header(), data.get("headerBase", {}))
            self._base(obj.get_content(), data.get("contentBase", {}))
            self._children(data.get("header", []), obj.get_header(), screen)
            self._children(data.get("children", []), obj.get_content(), screen)
        elif kind == "messagebox":
            if "title" in data:
                obj.add_title(data["title"])
            if "text" in data:
                obj.add_text(data["text"])
            for key, getter in (("header", "get_header"), ("content", "get_content"), ("footer", "get_footer")):
                records = data.get("headerButtons" if key == "header" else "children" if key == "content" else "footer", [])
                base = data.get(key + "Base", {})
                if key == "footer" and (records or base) and not obj.get_footer():
                    obj.add_footer_button("").delete()
                host = _call(obj, getter)
                if host is not None:
                    self._base(host, base)
                    self._children(records, host, screen, obj)
        elif kind == "imagebutton":
            for state, sources in enumerate(data.get("states", [])):
                current = [None, None, None]
                for slot, source in enumerate(sources):
                    def apply(image, index=slot, values=current, button=obj, native_state=state):
                        values[index] = image
                        button.set_src(native_state, values[0], values[1], values[2])
                    self._set_image_source(source, apply)
            self._children(data.get("children", []), obj, screen)
        elif kind == "scale":
            for key, method, default in (("mode", "set_mode", 0), ("ticks", "set_total_tick_count", 11),
                ("majorEvery", "set_major_tick_every", 5), ("rotation", "set_rotation", 0),
                ("angleRange", "set_angle_range", 270), ("showLabels", "set_label_show", True)):
                _call(obj, method, data.get(key, default))
            obj.set_range(data.get("min", 0), data.get("max", 100))
            if data.get("variant") == "gauge":
                needle = lv.line(obj)
                needle.set_style_line_color(lv.color_hex(data.get("needleColor", 0xeeeeee)), 0)
                needle.set_style_line_width(data.get("needleWidth", 3), 0)
                obj.set_line_needle_value(needle, data.get("needleLength", 50), data.get("value", 0))
            self._children(data.get("children", []), obj, screen)
        elif kind == "qrcode":
            if data.get("quietZone", False):
                raise ValueError("QR quiet zones must be rasterized by the MicroPython exporter")
            obj.set_size(data.get("size", 160))
            obj.set_dark_color(lv.color_hex(data.get("darkColor", 0)))
            obj.set_light_color(lv.color_hex(data.get("lightColor", 0xffffff)))
            content = data.get("data", "").encode("utf-8")
            obj.update(content, len(content))
        elif kind == "barcode":
            obj.set_dark_color(lv.color_hex(data.get("darkColor", 0)))
            obj.set_light_color(lv.color_hex(data.get("lightColor", 0xffffff)))
            obj.set_scale(data.get("scale", 2))
            obj.set_direction(data.get("direction", lv.DIR.HOR))
            obj.set_tiled(data.get("tiled", False))
            obj.set_encoding(data.get("encoding", 1))
            obj.update(data.get("data", "12345678"))
        elif kind == "page":
            _flag(obj, "scrollable", True)
            obj.set_scroll_dir(data.get("scrollDirection", lv.DIR.VER))
            obj.set_scrollbar_mode(data.get("scrollbarMode", lv.SCROLLBAR_MODE.AUTO))
            self._children(data.get("children", []), obj, screen)
        if not native and isinstance(data, dict):
            for part, selector in (("Main", lv.PART.MAIN), ("Items", lv.PART.ITEMS),
                                   ("Indicator", lv.PART.INDICATOR), ("Cursor", lv.PART.CURSOR)):
                if part in data.get("styles", {}):
                    self._legacy_style(obj, data["styles"][part], selector)

    def _button_map(self, obj, data):
        if "btnMap" in data:
            labels = list(data["btnMap"])
            if not labels or labels[-1] != "":
                labels.append("")
            obj.set_map(self._string_pointers(labels))
        if "nativeCtrl" in data:
            for index, flags in enumerate(data["nativeCtrl"]):
                obj.set_button_ctrl(index, flags)
        elif "ctrl" in data:
            for index, flags in enumerate(data["ctrl"]):
                # Legacy projects omit the native RECOLOR control bit, shifting
                # the remaining controls by one position after the width bits.
                obj.set_button_ctrl(index, (flags & 7) | ((flags & ~7) << 1))
        if "oneToggle" in data:
            obj.set_one_checked(data["oneToggle"])
        if "selected" in data:
            obj.set_selected_button(data["selected"])

    def _textarea_input_mode(self, obj, mode):
        if mode not in ("alphanumeric", "uppercase", "lowercase", "numeric"):
            raise ValueError("Text input mode must be alphanumeric, uppercase, lowercase or numeric")
        if mode == "alphanumeric":
            return
        # Import only for constrained fields. The generated tables share the
        # portable native helper's Unicode mappings; MicroPython str.upper and
        # str.lower alone do not provide Unicode conversion on every firmware.
        from uicreator_input_modes import normalize
        pointer = lv.C_Pointer()
        state = [False, ""]
        def constrain(event):
            if state[0]:
                return
            code = event.get_code()
            if code == lv.EVENT.INSERT:
                pointer.ptr_val = event.get_param()
                original = pointer.str_val
                if original == "\x7f":  # LVGL's delete marker is a control.
                    return
                replacement = normalize(mode, original, mode == "numeric" and "." in obj.get_text())
                if replacement != original:
                    state[1] = replacement  # LVGL borrows this through insert.
                    obj.set_insert_replace(replacement)
            elif code == lv.EVENT.VALUE_CHANGED:
                original = obj.get_text()
                replacement = normalize(mode, original)
                if replacement != original:
                    cursor = obj.get_cursor_pos()
                    state[0] = True
                    try:
                        obj.set_text(replacement)
                        obj.set_cursor_pos(len(normalize(mode, original[:cursor])))
                    finally:
                        state[0] = False
        self._on(obj, constrain, lv.EVENT.ALL)

    def _keyboard_input_mode(self, keyboard, mode, settings, original_map, cache):
        if mode == "alphanumeric":
            native_mode = settings.get("mode", 0)
            if original_map is not None:
                keyboard.set_map(native_mode, original_map[1], original_map[4])
            keyboard.set_mode(native_mode)
            return
        roots = cache.get(mode)
        if roots is not None:
            _KEYBOARD_MAP_ROOTS[7] = roots
            keyboard.set_map(7, roots[1], roots[4])
            keyboard.set_mode(7)
            return
        backspace, close, enter = lv.SYMBOL.BACKSPACE, lv.SYMBOL.CLOSE, lv.SYMBOL.OK
        if mode == "numeric":
            labels = ["1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n",
                      "0", ".", backspace, "\n", close, enter, ""]
        else:
            letters = "QWERTYUIOPASDFGHJKLZXCVBNM" if mode == "uppercase" else "qwertyuiopasdfghjklzxcvbnm"
            labels = list("1234567890") + ["\n"] + list(letters[:10]) + [backspace, "\n"] + \
                list(letters[10:19]) + [enter, "\n"] + list(letters[19:]) + ["\n", close, " ", ".", ",", ""]
        # Keep both native pointer arrays alive. USER_4 is reinstalled on each
        # target switch, so another UI replacing that global map is harmless.
        strings = self._string_pointers(labels)
        backing = array("i", [1] * sum(1 for label in labels if label not in ("", "\n")))
        view = memoryview(backing)
        controls = lv.C_Pointer.__cast__(view)
        roots = (labels, strings, backing, view, controls)
        self._resources.append(roots)
        cache[mode] = roots
        _KEYBOARD_MAP_ROOTS[7] = roots
        keyboard.set_map(7, strings, controls)
        keyboard.set_mode(7)

    def _string_pointers(self, names):
        pointers = lv.C_Pointer(len(names))
        for index, name in enumerate(names):
            pointers[index].str_val = name
        self._resources.extend((names, pointers))
        return pointers

    def _calendar(self, obj, data, apply_styles):
        header = obj.add_header_arrow()
        if "today" in data:
            date = data["today"]
            obj.set_today_date(date["year"], date["month"], date.get("day", 1))
        if "shown" in data:
            date = data["shown"]
            obj.set_month_shown(date["year"], date["month"])
        if "dayN" in data:
            names = list(data["dayN"])
            obj.set_day_names(self._string_pointers(names))
        if data.get("hlDates"):
            records = data["hlDates"]
            dates = lv.calendar_date_t(len(records))
            for index, date in enumerate(records):
                dates[index].year = date["year"]
                dates[index].month = date["month"]
                dates[index].day = date["day"]
            self._resources.append(dates)
            obj.set_highlighted_dates(dates, len(records))
        if apply_styles:
            matrix = obj.get_btnmatrix()
            for key, target, selector in (("styleBG", obj, lv.PART.MAIN),
                ("styleHead", header, lv.PART.MAIN), ("styleHeadPR", header, lv.PART.MAIN | lv.STATE.PRESSED),
                ("styleDayName", matrix, lv.PART.ITEMS), ("styleHDay", matrix, lv.PART.ITEMS | lv.STATE.CHECKED),
                ("styleInaDay", matrix, lv.PART.ITEMS | lv.STATE.DISABLED), ("weekBox", matrix, lv.PART.MAIN),
                ("todayBox", matrix, lv.PART.ITEMS | lv.STATE.FOCUSED)):
                if key in data:
                    self._legacy_style(target, data[key], selector)

    def _set_image_source(self, data, setter):
        if not isinstance(data, dict):
            setter(self._image(data))
            return
        frames = data.get("frames", [])
        if frames:
            descriptors = [self._image(frame if isinstance(frame, dict) else dict(data, mpyAsset=frame, frames=[])) for frame in frames]
            setter(descriptors[0])
            if len(descriptors) > 1:
                delays = data.get("delays", [100] * len(descriptors))
                state = [0]
                def animate(timer):
                    state[0] = (state[0] + 1) % len(descriptors)
                    setter(descriptors[state[0]])
                    timer.set_period(max(20, delays[state[0]]))
                self._callbacks.append(animate)
                self._timers.append(lv.timer_create(animate, max(20, delays[0]), None))
        elif "mpyAsset" in data or "sym" in data:
            setter(self._image(data))
        elif data:
            setter(self._image(data))

    def _configure_image(self, obj, data):
        self._set_image_source(data, obj.set_src)
        for key, method in (("offX", "set_offset_x"), ("offY", "set_offset_y"),
            ("rotation", "set_rotation"), ("scaleX", "set_scale_x"), ("scaleY", "set_scale_y"),
            ("antialias", "set_antialias"), ("blendMode", "set_blend_mode"), ("innerAlign", "set_inner_align")):
            if key in data:
                _call(obj, method, data[key])
        if "pivot" in data:
            obj.set_pivot(data["pivot"].get("x", 0), data["pivot"].get("y", 0))

    def _canvas(self, obj, data):
        width, height = data.get("width", 96), data.get("height", 64)
        color_format = data.get("format", lv.COLOR_FORMAT.ARGB8888)
        if "mpyAsset" in data:
            name = data["mpyAsset"]
            if "/" in name or "\\" in name:
                raise ValueError("Invalid canvas asset filename")
            with open(self.asset_root + "/" + name, "rb") as source:
                buffer = bytearray(source.read())
        else:
            size = width * height * (lv.color_format_get_bpp(color_format) // 8)
            buffer = bytearray(size)
        self._resources.append(buffer)
        obj.set_buffer(buffer, width, height, color_format)
        if "fillColor" in data and "mpyAsset" not in data:
            obj.fill_bg(lv.color_hex(data["fillColor"]), data.get("fillOpacity", 255))

    def _table(self, obj, data):
        if "columns" in data:
            obj.set_column_count(len(data["columns"]))
            for index, width in enumerate(data["columns"]):
                obj.set_column_width(index, width)
        if "rows" in data:
            obj.set_row_count(len(data["rows"]))
            for row, cells in enumerate(data["rows"]):
                for column, cell in enumerate(cells):
                    obj.set_cell_value(row, column, cell.get("text", ""))
                    if cell.get("flags"):
                        obj.set_cell_ctrl(row, column, cell["flags"])
        if "selectedRow" in data and "selectedColumn" in data:
            obj.set_selected_cell(data["selectedRow"], data["selectedColumn"])

    def _chart(self, obj, data):
        obj.set_type(data.get("type", 1))
        obj.set_point_count(data.get("pointCount", 10))
        obj.set_update_mode(data.get("updateMode", 0))
        obj.set_div_line_count(data.get("horizontalDivisions", 3), data.get("verticalDivisions", 5))
        for axis in data.get("axes", []):
            obj.set_axis_range(axis.get("axis", 0), axis.get("min", 0), axis.get("max", 100))
        series = []
        scatter = data.get("type") == 3
        for entry in data.get("series", []):
            handle = obj.add_series(lv.color_hex(entry.get("color", 0xffffff)), entry.get("axis", 0))
            series.append(handle)
            obj.hide_series(handle, entry.get("hidden", False))
            for index, value in enumerate(entry.get("y", [])):
                if index >= data.get("pointCount", 10):
                    break
                if scatter:
                    obj.set_series_value_by_id2(handle, index, entry.get("x", [0] * len(entry["y"]))[index], value)
                else:
                    obj.set_series_value_by_id(handle, index, value)
            obj.set_x_start_point(handle, entry.get("start", 0))
        for entry in reversed(data.get("cursors", [])):
            cursor = obj.add_cursor(lv.color_hex(entry.get("color", 0xffffff)), entry.get("directions", lv.DIR.ALL))
            if not entry.get("positionSet", True) and entry.get("series", -1) >= 0:
                obj.set_cursor_point(cursor, series[entry["series"]], entry.get("point", 0))
            else:
                point = lv.point_t({"x": entry.get("x", 0), "y": entry.get("y", 0)})
                self._resources.append(point)
                obj.set_cursor_pos(cursor, point)

    def _load_variables(self, records):
        for record in records:
            definition = record.get("var", record)
            name = definition["name"]
            if name in self.variables:
                raise ValueError("Duplicate variable: " + name)
            value = definition.get("default", 0)
            self._validate_value(definition, value)
            self.variables[name] = value
            self._definitions[name] = {"dataType": definition.get("dataType", 1), "code": definition.get("code", {})}
            self._histories[name] = [[], 0, definition.get("history", 1)]
            bindings = definition.get("bindings")
            if bindings is None:
                bindings = []
                if definition.get("oid", -1) >= 0 and definition.get("property"):
                    bindings = [{"screen": definition.get("screen", 0), "oid": definition["oid"],
                                 "property": definition["property"], "direction": "two-way"}]
            targets = []
            for binding in bindings:
                obj = self.get_object(binding.get("screen", 0), binding["oid"])
                path = binding["property"]
                direction = binding.get("direction", "two-way")
                targets.append((obj, path, direction))
                target_record = self._records[binding.get("screen", 0), binding["oid"]]
                if direction != "to-widget" and self._readable(path, target_record):
                    def read(event, variable=name, target=obj, property=path):
                        if not self._updating:
                            value = self._read(target, property)
                            kind = self._definitions[variable]["dataType"]
                            if kind == 0:
                                value = _text(value)
                            elif kind in (1, 4):
                                value = int(value)
                            elif kind == 2:
                                value = bool(value)
                            elif kind == 3:
                                value = float(value)
                            try:
                                self.set_variable(variable, value)
                            except ValueError:
                                if not self._advanced_path(property):
                                    raise
                                # A second bound control can reject an edit
                                # (for example a tighter date range). Restore
                                # this source without changing variable/history.
                                self._write(target, property, self.variables[variable])
                    self._on(obj, read, lv.EVENT.VALUE_CHANGED)
            self._bindings[name] = targets
        for name, value in self.variables.items():
            self._set_variable(name, value, False)
        for record in records:
            definition = record.get("var", record)
            for value in definition.get("value", []):
                self.set_variable(definition["name"], value)

    def _validate_value(self, definition, value):
        kind = definition.get("dataType", 1)
        valid = (kind == 0 and isinstance(value, str) and "\x00" not in value) or \
                (kind in (1, 4) and isinstance(value, int) and not isinstance(value, bool) and -(1 << 63) <= value < (1 << 63)) or \
                (kind == 2 and isinstance(value, bool)) or \
                (kind == 3 and isinstance(value, (int, float)) and not isinstance(value, bool) and value == value and abs(value) != _INF)
        if not valid:
            raise ValueError("Variable value has the wrong data type")
        code = definition.get("code", {}).get("cType", "")
        if kind == 1 and code in _INTEGER_RANGES and not _INTEGER_RANGES[code][0] <= value <= _INTEGER_RANGES[code][1]:
            raise ValueError("Variable value exceeds its configured storage range")
        if kind == 3 and code == "float" and abs(value) > 3.402823466e38:
            raise ValueError("Variable value exceeds float storage range")

    def get_variable(self, name):
        return self.variables[name]

    def get_history_count(self, name):
        return len(self._histories[name][0])

    def get_history(self, name, index=None):
        """Return an oldest-first copy, or one oldest-first entry by index.

        Defaults do not enter history. A configured history of zero grows;
        positive limits replace the oldest entry in O(1) after reaching capacity.
        """
        values, head, limit = self._histories[name]
        if index is None:
            return [values[(head + i) % len(values)] for i in range(len(values))]
        if not isinstance(index, int) or index < 0 or index >= len(values):
            raise IndexError("Variable history index is outside its stored entries")
        return values[(head + index) % len(values)]

    def set_variable(self, name, value):
        self._set_variable(name, value, True)

    def _set_variable(self, name, value, remember):
        definition = self._definitions[name]
        self._validate_value(definition, value)
        for obj, path, direction in self._bindings[name]:
            if direction != "from-widget" and self._advanced_path(path):
                self._advanced.validate(obj, value)
        if remember:
            history = self._histories[name]
            entries, head, limit = history
            if not limit or len(entries) < limit:
                entries.append(value)
            else:
                entries[head] = value
                history[1] = (head + 1) % limit
        self.variables[name] = value
        previous = self._updating
        self._updating = True
        try:
            for obj, path, direction in self._bindings[name]:
                if direction != "from-widget":
                    self._write(obj, path, value)
        finally:
            self._updating = previous

    def _readable(self, path, record):
        leaf = path.rsplit("/", 1)[-1]
        if leaf == "checked":
            return "cb" in record or "switch" in record or "checkboxButton" in record or "labeledCheckbox" in record or ("button" in record and
                (record["button"].get("toggle", False) or record.get("base", {}).get("flags", {}).get("checkable", False)))
        return leaf in ("value", "selected", "text", "date") and path != "/text"

    def _advanced_path(self, path):
        return path in ("/radioGroup/selected", "/menu/selected", "/checkboxButton/checked", "/labeledCheckbox/checked", "/datePicker/date")

    def _read(self, obj, path):
        if self._advanced_path(path):
            return self.get_advanced_value(obj)
        leaf = path.rsplit("/", 1)[-1]
        if leaf == "checked":
            return bool(obj.has_state(lv.STATE.CHECKED))
        if leaf in ("value", "selected", "text"):
            return _call(obj, "get_" + leaf)
        raise ValueError("Widget property is not readable: " + path)

    def _write(self, obj, path, value):
        if self._advanced_path(path):
            self.set_advanced_value(obj, value)
            return
        leaf = path.rsplit("/", 1)[-1]
        if leaf in ("checked", "hidden", "disabled"):
            _flag(obj, leaf, bool(value))
        elif leaf == "text":
            obj.set_text(_text(value))
        elif leaf in ("x", "y", "width", "height"):
            _call(obj, "set_" + leaf, int(value))
        elif leaf == "opascale":
            obj.set_style_opa(int(value), 0)
        elif leaf == "brightness":
            obj.set_brightness(int(value))
        elif leaf in ("value", "selected"):
            if path.startswith("/slider/") or path.startswith("/bar/") or path.startswith("/roller/"):
                _call(obj, "set_" + leaf, int(value), False)
            else:
                _call(obj, "set_" + leaf, int(value))
        else:
            raise ValueError("Unsupported variable property: " + path)

    def _bind_events(self, obj, screen, oid, actions):
        if not actions:
            return
        codes = {lv.EVENT.CLICKED: "clicked", lv.EVENT.VALUE_CHANGED: "value_changed", lv.EVENT.READY: "ready"}
        def dispatch(event):
            if self._dispatching or self._updating:
                return
            if event.get_target_obj() != event.get_current_target_obj():
                return
            event_name = codes.get(event.get_code())
            if event_name is None:
                return
            self._dispatching = True
            try:
                for action in actions:
                    if action.get("event", "clicked") == event_name and self._condition(action.get("condition")):
                        if not self._action(obj, screen, action):
                            break
            finally:
                self._dispatching = False
        self._on(obj, dispatch, lv.EVENT.ALL)

    def _condition(self, condition):
        if not condition:
            return True
        actual = self.get_variable(condition["variable"])
        expected = condition.get("value", True)
        op = condition.get("op", "eq")
        if op == "truthy":
            return bool(actual)
        if op == "eq":
            return actual == expected
        if op == "ne":
            return actual != expected
        return actual > expected if op == "gt" else actual < expected

    def _action(self, source, screen, action):
        kind = action["action"]
        if kind == "navigate":
            self.show_screen(action["screen"])
            return True
        if kind in ("set_variable", "increment_variable"):
            value = action.get("value", 1 if kind == "increment_variable" else 0)
            if kind == "increment_variable":
                value += self.get_variable(action["variable"])
            self.set_variable(action["variable"], value)
            return True
        oid = action.get("targetOid", -1)
        target = source if oid == -1 else self.get_object(action.get("targetScreen", screen), oid)
        if kind in ("show", "hide", "toggle"):
            hidden = kind == "hide" or (kind == "toggle" and not target.has_flag(lv.obj.FLAG.HIDDEN))
            _flag(target, "hidden", hidden)
        elif kind == "validate":
            value = target.get_text().strip()
            valid = bool(value) or not action.get("required", True)
            if valid and value and ("min" in action or "max" in action):
                try:
                    number = float(value)
                    valid = number == number and abs(number) != _INF and \
                        ("min" not in action or number >= action["min"]) and \
                        ("max" not in action or number <= action["max"])
                except ValueError:
                    valid = False
            target.set_style_border_color(lv.color_hex(0xd92d20), lv.STATE.USER_4)
            target.set_style_border_width(2, lv.STATE.USER_4)
            _flag(target, "state_user_4", not valid)
            if not valid:
                target.scroll_to_view_recursive(False)
            return valid
        else:
            raise ValueError("Unsupported event action: " + kind)
        return True

    def _link_keyboards(self):
        for screen, keyboard, settings in self._keyboards:
            target = settings.get("targetOid", -1)
            original_map = _KEYBOARD_MAP_ROOTS.get(settings.get("mode", 0))
            layouts = {}
            candidates = [(oid, textarea, mode) for owner, oid, textarea, mode in self._textareas
                          if owner == screen and (target < 0 or target == oid)]
            if target >= 0 and not candidates:
                raise ValueError("Keyboard target must be an existing text area")
            if target >= 0:
                keyboard.set_textarea(candidates[0][1])
                self._keyboard_input_mode(keyboard, candidates[0][2], settings, original_map, layouts)
            for oid, textarea, mode in candidates:
                def focus(event, kb=keyboard, entry=textarea, input_mode=mode, config=settings, layout=original_map, cache=layouts):
                    self._keyboard_input_mode(kb, input_mode, config, layout, cache)
                    kb.set_textarea(entry)
                    _flag(kb, "hidden", False)
                    kb.move_to_index(-1)
                    entry.scroll_to_view_recursive(False)
                self._on(textarea, focus, lv.EVENT.FOCUSED)
                self._on(textarea, focus, lv.EVENT.CLICKED)
            def close(event, kb=keyboard):
                kb.set_textarea(None)
                _flag(kb, "hidden", True)
            self._on(keyboard, close, lv.EVENT.READY)
            self._on(keyboard, close, lv.EVENT.CANCEL)
