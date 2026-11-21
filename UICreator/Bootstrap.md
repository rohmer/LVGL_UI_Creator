# Native Bootstrap-inspired components and starter layouts

The **Bootstrap** palette contains 36 native LVGL component recipes. **Templates**
contains 13 editable starter layouts. These use LVGL controls, styles and flex
layouts; there is no browser, HTML, CSS or JavaScript runtime. The components are
inspired by the documented [Bootstrap 5.3 component inventory](https://getbootstrap.com/docs/5.3/components/accordion/)
and [example layouts](https://getbootstrap.com/docs/5.3/examples/).

Click a component to insert it into the selected hierarchy parent. Templates
insert at the screen root. Each insertion is one undo step. Expand the hierarchy
to edit individual text, controls, colors and dimensions. Flex layouts control
child positions; change their layout/padding/sizing properties to rearrange them.
For a desktop-sized dashboard, first increase the simulator's screen dimensions.
The layouts also stack their metric cards at narrower sizes.

All recipe-owned widgets become ordinary document objects. JSON and device
Protobuf store the complete object trees, styles, native control state and
supported component actions. C export produces standard LVGL code and does not
require the recipe factory on the device. App-specific data, navigation,
authentication, payment processing, filtering and hardware actions are supplied
by application code.

## Component coverage

| Bootstrap component | Native implementation and behavior |
| --- | --- |
| Accordion | Multiple expandable sections; each header toggles its content. |
| Alerts | Styled message and dismiss control. |
| Badge | Compact styled text counter. |
| Breadcrumb | Editable navigation text; application supplies navigation. |
| Buttons | Styled native button, including pressed state. |
| Button group | Native checkable, mutually exclusive buttons. |
| Card | Editable title, body and action button. |
| Carousel | Native swipeable tabview slides with direct slide selection; no automatic cycling. |
| Close button | Native close-symbol button; attach the application's intended close action. |
| Collapse | Header toggles its content visibility. |
| Dropdowns | Native dropdown selection and popup. |
| List group | Native action-button list. |
| Modal | Reopenable, dismissible dialog composition inside its document parent; no browser backdrop or global input trap. |
| Navbar | Brand, actions and editable search field. |
| Navs and tabs | Native tabview with independent editable pages. |
| Offcanvas | Toggleable, dismissible drawer composition; no viewport-edge animation. |
| Pagination | Native exclusive page-selection controls; application supplies page data. |
| Placeholders | Editable skeleton bars; no loading service or shimmer animation. |
| Popovers | Touch/pointer-accessible toggleable hint content. |
| Progress | Native bar and editable progress caption. |
| Scrollspy | Section-navigation simulation using a vertical native tabview; no automatic tracking of arbitrary document scroll positions. |
| Spinners | Animated native spinner. |
| Toasts | Styled notification with dismiss control; no timeout or notification queue. |
| Tooltips | Toggleable hint card suitable for touch and pointer input; no automatic hover positioning. |

The form and admin additions are **Form controls**, **Input group**, **Floating
label**, **Checks and radios**, **Range**, **Validation**, **Data table**,
**Sidebar**, **Statistic card**, **Line chart**, **Bar chart** and **Activity feed**.
Text entry, password masking, dropdown selection, radio exclusivity, checkbox and
switch state, sliders, native table selection and chart data use native controls.
Floating labels use a persistent caption. Validation is an editable error-state
example with a disabled submit button; it does not validate application data.
Tables and charts contain illustrative sample data.

## Template coverage

| Starter layout | Included content |
| --- | --- |
| Admin dashboard | Navigation, three metric cards, line chart, records and activity. |
| Analytics dashboard | Metrics, trend chart, progress, records and activity. |
| Commerce dashboard | Metrics, sales bar chart, orders and activity. |
| User management | Search field, invite action and records table. |
| Settings page | Account fields, notifications, switch and radio group. |
| Sign in | Email, masked password, remember checkbox and sign-in action. |
| Checkout | Billing fields, sample total and continue action. |
| Pricing | Three editable plans with actions. |
| Album | Editable cards with media placeholders. |
| Blog | Article heading, text and related-content card. |
| Product | Hero content, action and feature card. |
| Cover | Introductory content and focused action. |
| Component gallery | All 36 component recipes in a scrollable reference screen. |

These are original native starter compositions. Bootstrap admin themes from
third-party publishers form an open-ended catalog; this library does not claim
to reproduce every commercial theme or its application functionality.

## Firmware and extension API

The vendored LVGL extension provides
`include/lvgl/widgets/lv_bootstrap.h`. Include it explicitly and call:

```c
lv_obj_t * dashboard = lv_bootstrap_create(parent, LV_BOOTSTRAP_DASHBOARD,
                                          false, NULL, NULL);
```

The `dark` argument selects the recipe's light or dark palette. An optional
visitor receives every recipe-owned object and its name, excluding native
control internals. Visitors must not delete or reparent objects during creation.
The editor uses this visitor to register the editable document tree.

The factory requires native label, button, checkbox, switch, dropdown, textarea,
table, chart, tabview, spinner, slider, bar and flex support. Minimal LVGL builds
without this full set still compile the module, but creation returns `NULL`.
Exported projects only require the controls listed in their generated
`ui_requirements.h`. The factory itself targets the bundled LVGL 9.6.

Two portable actions are stored as `base.bootstrapAction`: `dismiss` hides the
trigger's parent, and `collapse` toggles child index 1 of the trigger's parent.
Their recipe structure must remain intact when those actions are desired.
The editor loader, device runtime and C generator restore these actions without
retaining pointers to sibling objects. Adding arbitrary new actions requires
application code or an extension to the action schema.

`UICreatorBootstrap` exercises all 49 recipes, action behavior, JSON and Protobuf
round trips, preflight accounting, and C source export. Per-recipe generated
projects and JSON/Protobuf fixtures are written under the build's
`bootstrap-generated` directory for firmware compilation and runtime checks.
