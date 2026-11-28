"""Native LVGL composites for UI Creator's advanced form and menu controls.

No native extension is required. Callback and button-map storage is retained by
the owning UI, and all methods run on the LVGL thread.
"""
import lvgl as lv


def _flag(obj, flag, value):
    obj.add_flag(flag) if value else obj.remove_flag(flag)


def _state(obj, flag, value):
    obj.add_state(flag) if value else obj.remove_state(flag)


def _date(value):
    if not isinstance(value, str) or len(value) != 10 or value[4] != "-" or value[7] != "-":
        raise ValueError("Dates must use yyyy-mm-dd")
    try:
        year, month, day = int(value[:4]), int(value[5:7]), int(value[8:])
    except ValueError:
        raise ValueError("Invalid date")
    if not 1 <= year <= 9999 or not 1 <= month <= 12 or not 1 <= day <= _days(year, month):
        raise ValueError("Invalid calendar date")
    if value != "%04d-%02d-%02d" % (year, month, day):
        raise ValueError("Dates must use yyyy-mm-dd")
    return year, month, day


def _days(year, month):
    if month == 2:
        return 29 if year % 4 == 0 and (year % 100 != 0 or year % 400 == 0) else 28
    return 30 if month in (4, 6, 9, 11) else 31


def _weekday(year, month):
    # Sunday=0; Gregorian calendar, independent of the host's date/time library.
    offsets = (0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4)
    adjusted = year - (1 if month < 3 else 0)
    return (adjusted + adjusted // 4 - adjusted // 100 + adjusted // 400 + offsets[month - 1] + 1) % 7


class Controls:
    def __init__(self, owner):
        self.owner = owner
        self.entries = []

    def _column(self, obj):
        obj.set_flex_flow(lv.FLEX_FLOW.COLUMN)
        obj.set_style_pad_all(8, 0)
        obj.set_style_pad_row(6, 0)
        obj.set_style_pad_column(6, 0)

    def _button(self, parent, text):
        obj = lv.button(parent)
        obj.remove_flag(lv.obj.FLAG.SCROLLABLE)
        obj.set_height(38)
        obj.set_style_shadow_width(0, 0)
        caption = lv.label(obj)
        caption.set_text(text)
        caption.center()
        return obj, caption

    def _emit(self, root):
        root.send_event(lv.EVENT.VALUE_CHANGED, None)

    def _find(self, root):
        for obj, kind, data in self.entries:
            if obj == root:
                return kind, data
        raise ValueError("Object is not an advanced control in this UI")

    def part(self, root, name):
        return self._find(root)[1]["parts"].get(name)

    def get(self, root):
        kind, data = self._find(root)
        return data["selected"] if kind in ("radioGroup", "menu") else data["date"] if kind == "datePicker" else data["checked"]

    def validate(self, root, value):
        kind, data = self._find(root)
        if kind == "radioGroup":
            if not isinstance(value, int) or isinstance(value, bool) or value < -1 or value >= len(data["options"]) or (value >= 0 and data["options"][value].get("disabled", False)):
                raise ValueError("Radio selection must identify an enabled option")
        elif kind in ("checkboxButton", "labeledCheckbox"):
            if type(value) is not bool:
                raise ValueError("Checkbox value must be a boolean")
        elif kind == "datePicker":
            if value:
                _date(value)
                if (data.get("minDate") and value < data["minDate"]) or (data.get("maxDate") and value > data["maxDate"]):
                    raise ValueError("Date is outside the permitted range")
            elif value != "":
                raise ValueError("Date must be an ISO string")
        else:
            if not isinstance(value, int) or isinstance(value, bool) or value < 0 or (value and (value not in data["items"] or value in data["parents"])):
                raise ValueError("Menu selection must identify an enabled leaf")
            item_id = value
            while item_id:
                if data["items"][item_id].get("disabled", False):
                    raise ValueError("Menu item or its parent is disabled")
                item_id = data["items"][item_id].get("parent", 0)

    def set(self, root, value):
        self.validate(root, value)
        kind, data = self._find(root)
        if kind == "radioGroup":
            if not isinstance(value, int) or isinstance(value, bool) or value < -1 or value >= len(data["options"]):
                raise ValueError("Radio selection is outside the option list")
            if value >= 0 and data["options"][value].get("disabled", False):
                raise ValueError("Radio option is disabled")
            data["selected"] = value
            for index, button in enumerate(data["buttons"]):
                _state(button, lv.STATE.CHECKED, index == value)
        elif kind in ("checkboxButton", "labeledCheckbox"):
            if type(value) is not bool:
                raise ValueError("Checkbox value must be a boolean")
            data["checked"] = value
            _state(root, lv.STATE.CHECKED, value)
            _state(data["parts"]["Checkbox"], lv.STATE.CHECKED, value)
        elif kind == "datePicker":
            if value:
                _date(value)
                if (data.get("minDate") and value < data["minDate"]) or (data.get("maxDate") and value > data["maxDate"]):
                    raise ValueError("Date is outside the permitted range")
            elif value != "":
                raise ValueError("Date must be an ISO string")
            data["date"] = value
            display = value
            if value:
                year, month, day = _date(value)
                if data["format"] == "dd/mm/yyyy":
                    display = "%02d/%02d/%04d" % (day, month, year)
                elif data["format"] == "mm/dd/yyyy":
                    display = "%02d/%02d/%04d" % (month, day, year)
            data["parts"]["Label"].set_text(display or data["placeholder"])
        else:
            if not isinstance(value, int) or isinstance(value, bool):
                raise ValueError("Menu selection must be an item ID")
            if value and (value not in data["items"] or data["items"][value].get("disabled", False) or value in data["parents"]):
                raise ValueError("Menu selection must identify an enabled leaf")
            data["selected"] = value
            for item_id, button in data["buttons"].items():
                _state(button, lv.STATE.CHECKED, item_id == value)
            if value:
                data["page"] = data["items"][value].get("parent", 0)
            self._menu_page(data)

    def create(self, kind, parent, settings):
        data = dict(settings)
        data["parts"] = {}
        root = lv.obj(parent)
        self._column(root)
        self.entries.append((root, kind, data))
        if kind == "radioGroup":
            self._radio(root, data)
        elif kind in ("checkboxButton", "labeledCheckbox"):
            self._checkbox(root, data, kind == "labeledCheckbox")
        elif kind == "datePicker":
            self._date_picker(root, data)
        else:
            self._menu(root, data)
        return root

    def _radio(self, root, data):
        title = lv.label(root)
        title.set_text(data.get("title", "Location"))
        content = lv.obj(root)
        content.set_size(lv.pct(100), lv.SIZE_CONTENT)
        content.set_style_pad_all(0, 0)
        content.set_style_border_width(0, 0)
        content.set_flex_flow(lv.FLEX_FLOW.ROW_WRAP if data.get("horizontal", True) else lv.FLEX_FLOW.COLUMN)
        data["parts"].update(Title=title, Content=content)
        data["buttons"] = []
        data["options"] = data.get("options", [{"text": "Option 1"}, {"text": "Option 2"}])
        if not isinstance(data["options"], list):
            raise ValueError("Radio options must be an array")
        for index, option in enumerate(data["options"]):
            button, label = self._button(content, option.get("text", "Option"))
            button.add_flag(lv.obj.FLAG.CHECKABLE)
            _state(button, lv.STATE.DISABLED, option.get("disabled", False))
            data["buttons"].append(button)
            data["parts"]["Option" + str(index)] = button
            def choose(event, choice=index):
                if root.has_state(lv.STATE.DISABLED) or data["options"][choice].get("disabled", False):
                    self.set(root, data["selected"])
                    return
                self.set(root, choice)
                self._emit(root)
            self.owner._on(button, choose, lv.EVENT.CLICKED)
        self.set(root, data.get("selected", 0 if data["options"] else -1))

    def _checkbox(self, root, data, nested):
        checkbox = lv.checkbox(root)
        checkbox.set_text(data.get("text", "Option"))
        data["parts"].update(Checkbox=checkbox, Label=checkbox)
        if nested and data.get("description"):
            label = lv.label(root)
            label.set_text(data["description"])
            label.set_width(lv.pct(100))
            data["parts"]["Description"] = label
        root.add_flag(lv.obj.FLAG.CLICKABLE)
        _state(root, lv.STATE.DISABLED, data.get("disabled", False))
        _state(checkbox, lv.STATE.DISABLED, data.get("disabled", False))
        def toggle(event):
            if event.get_target_obj() != root or root.has_state(lv.STATE.DISABLED):
                return
            self.set(root, not data["checked"])
            self._emit(root)
        self.owner._on(root, toggle, lv.EVENT.CLICKED)
        def checked(event):
            if root.has_state(lv.STATE.DISABLED):
                self.set(root, data["checked"])
                return
            self.set(root, checkbox.has_state(lv.STATE.CHECKED))
            self._emit(root)
        self.owner._on(checkbox, checked, lv.EVENT.VALUE_CHANGED)
        self.set(root, data.get("checked", False))

    def _date_picker(self, root, data):
        data.setdefault("format", "yyyy-mm-dd")
        if data["format"] not in ("yyyy-mm-dd", "dd/mm/yyyy", "mm/dd/yyyy"):
            raise ValueError("Unknown date display format")
        data.setdefault("placeholder", "Select date")
        first = data.get("firstDay", 0)
        if not isinstance(first, int) or isinstance(first, bool) or not 0 <= first <= 6:
            raise ValueError("First day must be from 0 (Sunday) through 6")
        for field in ("minDate", "maxDate"):
            if data.get(field):
                _date(data[field])
        if data.get("minDate") and data.get("maxDate") and data["minDate"] > data["maxDate"]:
            raise ValueError("Minimum date exceeds maximum date")
        button, label = self._button(root, "")
        button.set_width(lv.pct(100))
        data["parts"].update(Display=button, Label=label)
        _state(root, lv.STATE.DISABLED, data.get("disabled", False))
        _state(button, lv.STATE.DISABLED, data.get("disabled", False))
        data["popup"] = None
        self.set(root, data.get("date", ""))
        self.owner._on(button, lambda event: self.open_date(root), lv.EVENT.CLICKED)
        def deleting(event):
            if data["popup"] is not None:
                popup, data["popup"] = data["popup"], None
                popup.delete()
        self.owner._on(root, deleting, lv.EVENT.DELETE)

    def close_date(self, root):
        kind, data = self._find(root)
        if kind != "datePicker":
            raise ValueError("Object is not a date picker")
        if data["popup"] is not None:
            data["popup"].add_flag(lv.obj.FLAG.HIDDEN)

    def open_date(self, root):
        kind, data = self._find(root)
        if kind != "datePicker":
            raise ValueError("Object is not a date picker")
        if root.has_state(lv.STATE.DISABLED):
            return
        if data["popup"] is None:
            screen = root.get_screen()
            screen.update_layout()
            popup = lv.obj(screen)
            data["popup"] = popup
            popup.set_size(lv.pct(100), lv.pct(100))
            popup.set_style_bg_opa(96, 0)
            popup.set_style_pad_all(0, 0)
            popup.remove_flag(lv.obj.FLAG.SCROLLABLE)
            popup.add_flag(lv.obj.FLAG.CLICKABLE)
            calendar = lv.obj(popup)
            calendar.set_size(max(1, min(280, screen.get_width() - 16)),
                              max(1, min(290, screen.get_height() - 16)))
            calendar.center()
            self._column(calendar)
            header = lv.obj(calendar)
            header.set_size(lv.pct(100), 40)
            header.set_style_pad_all(0, 0)
            header.set_flex_flow(lv.FLEX_FLOW.ROW)
            previous, unused = self._button(header, "<")
            previous.set_width(36)
            title = lv.label(header)
            title.set_flex_grow(1)
            following, unused = self._button(header, ">")
            following.set_width(36)
            matrix = lv.buttonmatrix(calendar)
            matrix.set_size(lv.pct(100), 1)
            matrix.set_flex_grow(1)
            data["parts"].update(Popup=popup, Calendar=calendar, Month=title, Previous=previous, Next=following, Days=matrix)
            self.owner._on(previous, lambda event: self._month(root, -1), lv.EVENT.CLICKED)
            self.owner._on(following, lambda event: self._month(root, 1), lv.EVENT.CLICKED)
            def outside(event):
                if event.get_target_obj() == popup:
                    self.close_date(root)
            self.owner._on(popup, outside, lv.EVENT.CLICKED)
            def escape(event):
                if event.get_key() == lv.KEY.ESC:
                    self.close_date(root)
            self.owner._on(popup, escape, lv.EVENT.KEY)
            self.owner._on(matrix, escape, lv.EVENT.KEY)
            def select(event):
                index = matrix.get_selected_button()
                if index >= len(data["dates"]) or not data["dates"][index]:
                    return
                value = data["dates"][index]
                if (data.get("minDate") and value < data["minDate"]) or (data.get("maxDate") and value > data["maxDate"]):
                    return
                self.set(root, value)
                self.close_date(root)
                self._emit(root)
            self.owner._on(matrix, select, lv.EVENT.VALUE_CHANGED)
        initial = data["date"] or data.get("minDate") or "2026-01-01"
        data["year"], data["month"], unused = _date(initial)
        self._calendar(root)
        data["popup"].remove_flag(lv.obj.FLAG.HIDDEN)
        data["popup"].move_to_index(-1)

    def _month(self, root, delta):
        unused, data = self._find(root)
        month = data["year"] * 12 + data["month"] - 1 + delta
        if not 12 <= month < 120000:
            return
        data["year"], data["month"] = month // 12, month % 12 + 1
        self._calendar(root)

    def _calendar(self, root):
        unused, data = self._find(root)
        year, month, first = data["year"], data["month"], data.get("firstDay", 0)
        data["parts"]["Month"].set_text("%04d-%02d" % (year, month))
        labels, values = [], []
        days = ("Su", "Mo", "Tu", "We", "Th", "Fr", "Sa")
        for column in range(7):
            labels.append(days[(column + first) % 7])
            values.append("")
        labels.append("\n")
        start = (_weekday(year, month) - first) % 7
        for cell in range(42):
            day = cell - start + 1
            valid = 1 <= day <= _days(year, month)
            labels.append(str(day) if valid else " ")
            values.append("%04d-%02d-%02d" % (year, month, day) if valid else "")
            if cell % 7 == 6 and cell != 41:
                labels.append("\n")
        labels.append("")
        # LVGL borrows this map. Replace the retained roots after set_map so GC
        # can release the previous month without accumulating allocations.
        pointers = self.owner._string_pointers(labels)
        matrix = data["parts"]["Days"]
        matrix.set_map(pointers)
        old_roots = data.get("mapRoots")
        data["mapRoots"] = (labels, pointers)
        if old_roots:
            self.owner._resources.remove(old_roots[0])
            self.owner._resources.remove(old_roots[1])
        data["dates"] = values
        matrix.set_one_checked(True)
        for index, value in enumerate(values):
            matrix.clear_button_ctrl(index, lv.buttonmatrix.CTRL.DISABLED | lv.buttonmatrix.CTRL.CHECKED)
            if not value or (data.get("minDate") and value < data["minDate"]) or (data.get("maxDate") and value > data["maxDate"]):
                matrix.set_button_ctrl(index, lv.buttonmatrix.CTRL.DISABLED)
            elif value == data["date"]:
                matrix.set_button_ctrl(index, lv.buttonmatrix.CTRL.CHECKED)

    def _menu(self, root, data):
        title = lv.label(root)
        title.set_text(data.get("title", "Menu"))
        back, unused = self._button(root, "< Back")
        content = lv.obj(root)
        content.set_size(lv.pct(100), lv.pct(100))
        self._column(content)
        data["parts"].update(Title=title, Content=content, Back=back)
        entries = data.get("items", [])
        data["items"], data["buttons"], data["parents"] = {}, {}, set()
        for item in entries:
            item_id, parent = item.get("id", 0), item.get("parent", 0)
            if not isinstance(item_id, int) or isinstance(item_id, bool) or item_id <= 0 or item_id > 2147483647 or item_id in data["items"]:
                raise ValueError("Menu item IDs must be positive and unique")
            data["items"][item_id] = item
            if parent:
                data["parents"].add(parent)
        for item_id, item in data["items"].items():
            visited, parent = {item_id}, item.get("parent", 0)
            while parent:
                if parent not in data["items"] or parent in visited:
                    raise ValueError("Menu parent is missing or cyclic")
                visited.add(parent)
                parent = data["items"][parent].get("parent", 0)
        for item_id, item in data["items"].items():
            button, unused = self._button(content, item.get("text", "Item") + (" >" if item_id in data["parents"] else ""))
            button.set_width(lv.pct(100))
            _state(button, lv.STATE.DISABLED, item.get("disabled", False))
            data["buttons"][item_id] = button
            data["parts"]["Item" + str(item_id)] = button
            def choose(event, selected=item_id):
                if root.has_state(lv.STATE.DISABLED) or data["items"][selected].get("disabled", False):
                    return
                if selected in data["parents"]:
                    data["page"] = selected
                    self._menu_page(data)
                else:
                    self.set(root, selected)
                    self._emit(root)
            self.owner._on(button, choose, lv.EVENT.CLICKED)
        def previous(event):
            data["page"] = data["items"].get(data["page"], {}).get("parent", 0)
            self._menu_page(data)
        self.owner._on(back, previous, lv.EVENT.CLICKED)
        data["page"] = 0
        self.set(root, data.get("selected", 0))
        self._menu_page(data)

    def _menu_page(self, data):
        data["parts"]["Title"].set_text(data["items"][data["page"]].get("text", "Item") if data["page"] else data.get("title", "Menu"))
        _flag(data["parts"]["Back"], lv.obj.FLAG.HIDDEN, not data["page"])
        for item_id, button in data["buttons"].items():
            _flag(button, lv.obj.FLAG.HIDDEN, data["items"][item_id].get("parent", 0) != data["page"])
