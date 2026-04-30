import SwiftUI

// G3 — Moving Head fixture selection row. Desktop's
// `MovingHeadPanel` surfaces eight checkboxes (MH1-MH8) that pick
// which fixture slots this effect targets; iPad needs the same
// affordance but in a touch-friendly grid layout.
//
// A fixture is "active" iff its `E_TEXTCTRL_MH<n>_Settings` string
// is non-empty — desktop derives checkbox state the same way at
// open time (`MovingHeadPanel.cpp:1974-1985`). Toggling writes an
// initial command string (activate) or clears it (deactivate);
// the bridge then rewrites every active fixture's Pan / Tilt /
// Offsets / Groupings / Cycles from the current slider values so
// the `Heads:` list the renderer uses stays in sync with the new
// selection.
struct MovingHeadFixtureRowView: View {
    @Environment(SequencerViewModel.self) var viewModel

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("Fixtures")
                .font(.caption)
                .foregroundStyle(.secondary)
            // Two rows of four so all eight fit in the 280pt
            // inspector without wrapping on smaller screens.
            HStack(spacing: 6) {
                ForEach(1...4, id: \.self) { i in
                    fixtureButton(i)
                }
            }
            HStack(spacing: 6) {
                ForEach(5...8, id: \.self) { i in
                    fixtureButton(i)
                }
            }
        }
        .padding(.vertical, 4)
    }

    @ViewBuilder
    private func fixtureButton(_ i: Int) -> some View {
        let active = isActive(i)
        Button {
            toggle(i)
        } label: {
            Text("\(i)")
                .font(.caption)
                .fontWeight(.semibold)
                .frame(maxWidth: .infinity, minHeight: 28)
                .foregroundStyle(active ? Color.white : Color.primary)
                .background(
                    RoundedRectangle(cornerRadius: 6)
                        .fill(active
                              ? Color.accentColor
                              : Color.secondary.opacity(0.15))
                )
        }
        .buttonStyle(.plain)
    }

    // MARK: - State

    /// True when fixture i's `E_TEXTCTRL_MH<i>_Settings` entry is
    /// non-empty. Read via the mask the bridge exposes —
    /// inspectorRevision ensures SwiftUI recomputes the button
    /// state after a slider edit triggers a sync write.
    private func isActive(_ i: Int) -> Bool {
        _ = viewModel.inspectorRevision
        guard let sel = viewModel.selectedEffect else { return false }
        let mask = Int(viewModel.document.movingHeadActiveFixtures(
            forRow: Int32(sel.rowIndex),
            at: Int32(sel.effectIndex)))
        return (mask & (1 << (i - 1))) != 0
    }

    private func toggle(_ i: Int) {
        guard let sel = viewModel.selectedEffect else { return }
        let nowActive = !isActive(i)
        _ = viewModel.document.setMovingHeadFixture(
            Int32(i),
            active: nowActive,
            forRow: Int32(sel.rowIndex),
            at: Int32(sel.effectIndex))
        // The bridge mutated the settings map directly (bypassing
        // `setEffectSettingValue` so we didn't register N undo
        // steps); refresh the Swift observable so the buttons and
        // any downstream readers pick up the new MH*_Settings
        // state, and kick a render so the scrub pane updates.
        viewModel.refreshSelectedEffectSettings()
        viewModel.inspectorRevision &+= 1
    }
}

/// Read-only banner above the Moving Head fixture row. Flags the
/// partial-authoring story iPad users need to be aware of — colour
/// wheel, dimmer canvas, and path drawing remain desktop-only.
struct MovingHeadInfoRowView: View {
    var body: some View {
        HStack(alignment: .top, spacing: 6) {
            Image(systemName: "info.circle")
                .foregroundStyle(.tint)
                .font(.caption)
            Text("Colour, dimmer, and path authoring still require the desktop Effect Assist panel. iPad supports fixture selection + Pan / Tilt / Offset / Groupings / Cycles edits.")
                .font(.caption2)
                .foregroundStyle(.secondary)
        }
        .padding(.vertical, 4)
    }
}
