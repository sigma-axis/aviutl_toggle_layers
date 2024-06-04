/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <algorithm>
#include <set>
#include <ranges>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>
using namespace ExEdit;

////////////////////////////////
// 主要情報源の変数アドレス．
////////////////////////////////
inline constinit struct ExEdit092 {
	AviUtl::FilterPlugin* fp;
	decltype(fp->func_WndProc) func_wndproc;
	constexpr static const char* info_exedit092 = "拡張編集(exedit) version 0.92 by ＫＥＮくん";
	bool init(AviUtl::FilterPlugin* this_fp)
	{
		if (fp != nullptr) return true;
		AviUtl::SysInfo si; this_fp->exfunc->get_sys_info(nullptr, &si);
		for (int i = 0; i < si.filter_n; i++) {
			auto that_fp = this_fp->exfunc->get_filterp(i);
			if (that_fp->information != nullptr &&
				0 == std::strcmp(that_fp->information, info_exedit092)) {
				fp = that_fp;
				func_wndproc = fp->func_WndProc;
				init_pointers();
				return true;
			}
		}
		return false;
	}

	LayerSetting*	LayerSettings;				// 0x188498
	int32_t*	current_scene;					// 0x1a5310
	int32_t*	curr_timeline_layer_height;		// 0x0a3e20
	HWND*		timeline_v_scroll_bar;			// 0x158d34
	int32_t*	timeline_v_scroll_pos;			// 0x1a5308
	int32_t*	timeline_height_in_layers;		// 0x0a3fbc

	int32_t*	SortedObjectLayerBeginIndex;	// 0x149670
	int32_t*	SortedObjectLayerEndIndex;		// 0x135ac8
	Object**	SortedObject;					// 0x168fa8
	Object**	ObjectArray_ptr;				// 0x1e0fa4
	int32_t*	SelectingObjectNum_ptr;			// 0x167d88
	int32_t*	SelectingObjectIndex;			// 0x179230

	//int32_t*	last_clicked_x;					// 0x1460b4
	int32_t*	last_clicked_y;					// 0x196744

	void(*nextundo)();							// 0x08d150
	void(*setundo)(uint32_t, uint32_t);			// 0x08d290

private:
	void init_pointers()
	{
		auto pick_addr = [this, exedit_base = reinterpret_cast<uintptr_t>(fp->dll_hinst)]
			<class T>(T & target, ptrdiff_t offset) { target = reinterpret_cast<T>(exedit_base + offset); };
		pick_addr(LayerSettings,				0x188498);
		pick_addr(current_scene,				0x1a5310);
		pick_addr(curr_timeline_layer_height,	0x0a3e20);
		pick_addr(timeline_v_scroll_bar,		0x158d34);
		pick_addr(timeline_v_scroll_pos,		0x1a5308);
		pick_addr(timeline_height_in_layers,	0x0a3fbc);

		pick_addr(SortedObjectLayerBeginIndex,	0x149670);
		pick_addr(SortedObjectLayerEndIndex,	0x135ac8);
		pick_addr(SortedObject,					0x168fa8);
		pick_addr(ObjectArray_ptr,				0x1e0fa4);
		pick_addr(SelectingObjectNum_ptr,		0x167d88);
		pick_addr(SelectingObjectIndex,			0x179230);

		//pick_addr(last_clicked_x				0x1460b4);
		pick_addr(last_clicked_y,				0x196744);

		pick_addr(nextundo,						0x08d150);
		pick_addr(setundo,						0x08d290);
	}
} exedit;


////////////////////////////////
// 縦スクロールバー操作．
////////////////////////////////
constexpr struct LayerScrollBar {
	void set_pos(int pos, AviUtl::EditHandle* editp) const
	{
		auto hwnd = *exedit.timeline_v_scroll_bar;
		if (hwnd == nullptr) return;

		const SCROLLINFO si{ .cbSize = sizeof(si), .fMask = SIF_POS, .nPos = pos };
		::SetScrollInfo(hwnd, SB_CTL, &si, true);
		exedit.fp->func_WndProc(exedit.fp->hwnd, WM_VSCROLL,
			(pos << 16) | SB_THUMBTRACK, reinterpret_cast<LPARAM>(hwnd),
			editp, exedit.fp);
	}
	int get_pos() const { return *exedit.timeline_v_scroll_pos; }
	int get_page_size() const { return *exedit.timeline_height_in_layers; }
} tl_scroll_v;


////////////////////////////////
// 各レイヤーに対する操作．
////////////////////////////////
struct layer_operation {
	constexpr static size_t num_layers = 100;
	constexpr static int x_leftmost_timeline = 64, y_topmost_timeline = 42;

	static bool point_in_header(int x, int y) {
		return x < x_leftmost_timeline && y >= y_topmost_timeline;
	}
	// シーンによらず最上段レイヤーは 0 扱い．
	static int point_to_layer(int y) {
		y -= y_topmost_timeline;
		y = floor_div(y, *exedit.curr_timeline_layer_height);
		y += *exedit.timeline_v_scroll_pos;

		return std::clamp<int>(y, 0, num_layers - 1);
	}
	// division that rounds toward negative infinity.
	// `divisor` is assumed to be positive.
	static constexpr auto floor_div(auto dividend, auto const divisor) {
		if (dividend < 0) dividend -= divisor - 1;
		return dividend / divisor;
	}
	// returns y-coordinate of the top side of the layer.
	static int layer_to_point(int layer) {
		layer -= *exedit.timeline_v_scroll_pos;
		layer *= *exedit.curr_timeline_layer_height;
		layer += y_topmost_timeline;
		return layer;
	}

	static auto& layer_flags(int layer) {
		return exedit.LayerSettings[layer + num_layers * (*exedit.current_scene)].flag;
	}
	static void set_layer_undo(int layer) { exedit.setundo(layer, 0x10); }

	virtual bool initialize(int layer, AviUtl::EditHandle* editp) const = 0;
	virtual bool notify() const { return false; }

protected:
	static std::pair<int, int> prev_curr_to_from_until(int prev, int curr) {
		// find the range to check.
		if (curr < prev) return { curr, prev };
		else return { prev + 1, curr + 1 };
	}
};

struct layer_drag_operation : layer_operation {
	static inline constinit bool flagging = false;
	virtual bool set(int layer_prev, int layer_curr, WPARAM wparam) const = 0;

};

// 各種フラグ操作．
template<LayerSetting::Flag flag, bool redraw_screen, bool redraw_entire_tl>
struct layer_op_flags : layer_drag_operation {
	bool initialize(int layer, AviUtl::EditHandle* editp) const override
	{
		auto& clicked_flag = layer_flags(layer);

		// prepare and push to the undo buffer.
		exedit.nextundo();
		set_layer_undo(layer);

		// update the clicked layer.
		flagging = !has_flag_or(clicked_flag, flag);
		clicked_flag ^= flag;

		// always results in updates.
		return true;
	}
	bool set(int layer_prev, int layer_curr, WPARAM) const override
	{
		auto [from, until] = prev_curr_to_from_until(layer_prev, layer_curr);

		bool updated = false;
		for (int l = from; l < until; l++) updated |= set(l);
		return updated;
	}

	bool notify() const override
	{
		if constexpr (redraw_entire_tl) {
			// redraw the entire timeline.
			::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);
		}
		else {
			// redraw the header part of the timeline.
			RECT rc; ::GetClientRect(exedit.fp->hwnd, &rc);
			rc.right = x_leftmost_timeline; rc.top = y_topmost_timeline;
			::InvalidateRect(exedit.fp->hwnd, &rc, FALSE);
		}

		// returns true if the image should be updated.
		return redraw_screen;
	}

private:
	static bool set(int layer)
	{
		if (auto& flags = layer_flags(layer);
			has_flag_or(flags, flag) ^ flagging) {

			// push undo buffer.
			set_layer_undo(layer);

			// modify the flag.
			flags ^= flag;

			// flag did change.
			return true;
		}
		// flag didn't change.
		return false;
	}
};
constexpr layer_op_flags<LayerSetting::Flag::UnDisp, true, true> op_undisp;
constexpr layer_op_flags<LayerSetting::Flag::Locked, false, false> op_locked;
constexpr layer_op_flags<LayerSetting::Flag::CoordLink, false, false> op_coord_link;
constexpr layer_op_flags<LayerSetting::Flag::Clip, true, false> op_clip;

// オブジェクト選択．
constexpr struct : layer_drag_operation {
private:
	static int sorted_to_idx(int j) { return exedit.SortedObject[j] - *exedit.ObjectArray_ptr; }
	static std::set<int> get_selected() {
		// store the selected index to std::set<int>.
		return { exedit.SelectingObjectIndex, exedit.SelectingObjectIndex + *exedit.SelectingObjectNum_ptr };
	}
	static void set_selected(std::set<int>& sel) {
		// copy back the selected index to exedit.
		std::copy(sel.begin(), sel.end(), exedit.SelectingObjectIndex);
		*exedit.SelectingObjectNum_ptr = sel.size();
	}

public:
	bool initialize(int layer, AviUtl::EditHandle* editp) const override
	{
		auto sel = get_selected();

		// see whether all objects in the layer is selected.
		flagging = true; // empty layer should turn to "flagging" mode.
		for (int j = exedit.SortedObjectLayerBeginIndex[layer], r = exedit.SortedObjectLayerEndIndex[layer];
			j <= r; j++) {
			if (sel.contains(sorted_to_idx(j))) flagging = false;
			else {
				flagging = true;
				break;
			}
		}

		// then apply the operation to the layer.
		(flagging ? add : remove)(layer, sel);

		// if any additions/removals took place, set them back to exedit.
		if (sel.size() == *exedit.SelectingObjectNum_ptr) return false;
		set_selected(sel);
		return true;
	}
	bool set(int layer_prev, int layer_curr, WPARAM wparam) const override
	{
		// nothing to do when ctrl isn't pressed.
		if ((wparam & MK_CONTROL) == 0) return false;

		auto [from, until] = prev_curr_to_from_until(layer_prev, layer_curr);

		// update each layer.
		auto sel = get_selected();
		auto& op = flagging ? add : remove;
		for (int l = from; l < until; l++) op(l, sel);

		// if any additions/removals took place, set them back to exedit.
		if (sel.size() == *exedit.SelectingObjectNum_ptr) return false;
		set_selected(sel);
		return true;
	}

private:
	static void add(int layer, std::set<int>& sel)
	{
		// avoid selecting objects in locked layers.
		if (has_flag_or(layer_flags(layer), LayerSetting::Flag::Locked)) return;

		for (int j = exedit.SortedObjectLayerBeginIndex[layer], r = exedit.SortedObjectLayerEndIndex[layer];
			j <= r; j++) sel.insert(sorted_to_idx(j));
	}
	static void remove(int layer, std::set<int>& sel)
	{
		for (int j = exedit.SortedObjectLayerBeginIndex[layer], r = exedit.SortedObjectLayerEndIndex[layer];
			j <= r; j++) sel.erase(sorted_to_idx(j));
	}

public:
	bool notify() const override
	{
		// redraw the entire timeline.
		::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);

		// the image remains unchanged.
		return false;
	}
} op_select;

// 右クリックコマンドの発動．
template<uint32_t id>
struct layer_command_operation : layer_operation {
	constexpr static uint32_t command_id = id;
	bool initialize(int layer, AviUtl::EditHandle* editp) const override {
		*exedit.last_clicked_y = layer_to_point(layer);
		return exedit.func_wndproc(exedit.fp->hwnd, WM_COMMAND, command_id, 0, editp, exedit.fp) != FALSE;
	}
};

constexpr static uint32_t
	layer_command_id_rename = 1056,
	layer_command_id_toggle_others = 1075;
constexpr layer_command_operation<layer_command_id_rename> op_rename;
constexpr layer_command_operation<layer_command_id_toggle_others> op_toggle_others;


////////////////////////////////
// 設定項目．
////////////////////////////////
enum class mod_key : uint8_t {
	no_keys = 0,
	ctrl  = 1 << 0,
	shift = 1 << 1,
	alt   = 1 << 2,
};
template<> struct AviUtl::detail::flag::ops_def<mod_key> :std::true_type {};
enum class layer_op_kind : uint8_t {
	none          = 0,
	undisp        = 1,
	locked        = 2,
	coord_link    = 3,
	clip          = 4,
	select        = 5,
	rename        = 6,
	toggle_others = 7,
};
inline constinit struct Settings {
private:
	static constexpr const char* mod_key_name(mod_key keys) {
		switch (keys) {
			using enum mod_key;
		case no_keys:				return "no_keys";
		case ctrl:					return "ctrl";
		case shift:					return "shift";
		case ctrl | shift:			return "ctrl_shift";
		case alt:					return "alt";
		case ctrl | alt:			return "ctrl_alt";
		case shift | alt:			return "shift_alt";
		case ctrl | shift | alt:	return "ctrl_shift_alt";
		}
		std::unreachable();
	}

public:
	void load(const char* inifile)
	{
		for (auto [k, v] : mapping | std::views::enumerate) {
			auto key = static_cast<mod_key>(k);
			auto val = static_cast<layer_op_kind>(::GetPrivateProfileIntA(
				"key_combination", mod_key_name(key), static_cast<int>(v), inifile));

			// select operation requires ctrl key pressed.
			if (val == layer_op_kind::select && !has_flag_or(key, mod_key::ctrl))
				v = layer_op_kind::none;
			else v = val;
		}
	}

	layer_operation const* map(mod_key keys) const
	{
		auto i = static_cast<size_t>(keys);
		if (i >= std::size(mapping)) return nullptr;

		auto j = static_cast<size_t>(mapping[i]);
		if (j >= std::size(kind_to_ptr)) return nullptr;

		return kind_to_ptr[j];
	}

private:
	static constexpr layer_operation const* kind_to_ptr[]{
		nullptr,
		&op_undisp,
		&op_locked,
		&op_coord_link,
		&op_clip,
		&op_select,
		&op_rename,
		&op_toggle_others,
	};
	layer_op_kind mapping[8]{
		layer_op_kind::undisp,	// no mod key.
		layer_op_kind::select,	// ctrl.
		layer_op_kind::locked,	// shift.
		layer_op_kind::none,	// ctrl+shift.
		layer_op_kind::rename,	// alt.
		layer_op_kind::none,	// ctrl+alt.
		layer_op_kind::none,	// shift+alt.
		layer_op_kind::none,	// ctrl+shift+alt.
	};
} settings;


////////////////////////////////
// ExEdit のコールバック乗っ取り．
////////////////////////////////
class Drag {
	static inline constinit layer_drag_operation const* curr_operation = nullptr;
	static inline constinit int layer_prev = 0;

	static bool on_drag(int layer_mouse, WPARAM wparam, AviUtl::EditHandle* editp)
	{
		// scroll vertically if the mouse is outside the window.
		scroll_vertically_on_drag(layer_mouse, editp);

		{
			// limit the target layers to the visible ones.
			auto top_layer = tl_scroll_v.get_pos();
			layer_mouse = std::clamp(layer_mouse, top_layer, top_layer + tl_scroll_v.get_page_size() - 1);
		}

		// return if the position didn't change.
		if (layer_mouse == layer_prev) return false;

		// update the previous mouse position.
		auto prev = layer_prev;
		layer_prev = layer_mouse;

		return curr_operation->set(prev, layer_mouse, wparam);
	}

	static void exit_drag(HWND hwnd) {
		curr_operation = nullptr;
		if (::GetCapture() == hwnd)
			::ReleaseCapture();
	}

	// scrolls the timeline vertically when dragging over the visible area.
	// there are at least 100 ms intervals between scrolls.
	static void scroll_vertically_on_drag(int layer_mouse, AviUtl::EditHandle* editp) {
		constexpr uint32_t interval_min = 100;

		constexpr auto check_tick = [] {
		#pragma warning(suppress : 28159) // 32 bit is enough.
			auto curr = ::GetTickCount();

			static constinit decltype(curr) prev = 0;
			if (curr - prev >= interval_min) {
				prev = curr;
				return true;
			}
			return false;
		};

		int dir;
		if (auto rel_pos = layer_mouse - tl_scroll_v.get_pos();
			rel_pos < 0) dir = -1;
		else if (rel_pos >= tl_scroll_v.get_page_size()) dir = +1;
		else return;
		if (!check_tick()) return;

		// then scroll.
		tl_scroll_v.set_pos(tl_scroll_v.get_pos() + dir, editp);
	}

	static layer_operation const* choose_operation(WPARAM wparam) {
		if (wparam & ~(MK_LBUTTON | MK_CONTROL | MK_SHIFT)) return nullptr;

		using enum mod_key;
		mod_key keys = no_keys;
		if ((wparam & MK_CONTROL) != 0) keys |= ctrl;
		if ((wparam & MK_SHIFT) != 0) keys |= shift;
		if (::GetKeyState(VK_MENU) < 0) keys |= alt;
		return settings.map(keys);
	}

public:
	static BOOL func_wndproc_detour(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp)
	{
		if (curr_operation != nullptr) {
			if (!fp->exfunc->is_editing(editp) || fp->exfunc->is_saving(editp)) {
				// AviUtl isn't in a suitable state. abort dragging.
				exit_drag(hwnd);
				goto default_handler;
			}

			switch (message) {
			case WM_MOUSEMOVE:
				return on_drag(layer_operation::point_to_layer(static_cast<int16_t>(lparam >> 16)), wparam, editp)
					&& curr_operation->notify() ? TRUE : FALSE;

			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_MBUTTONDOWN:
			case WM_XBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_RBUTTONUP:
			case WM_MBUTTONUP:
			case WM_XBUTTONUP:
				// any mouse button would stop this drag.
			case WM_CAPTURECHANGED: // drag aborted.
				exit_drag(hwnd);
				return FALSE;

			default:
				// otherwise let it be handled normally.
				goto default_handler;
			}
		}
		else {
			// assume left click with no modifier keys other than shift.
			if (message != WM_LBUTTONDOWN) goto default_handler;

			layer_operation const* cand_operation = choose_operation(wparam);
			if (cand_operation == nullptr) goto default_handler;

			// the mouse should be on a layer header.
			int mouse_x = static_cast<int16_t>(lparam), mouse_y = static_cast<int16_t>(lparam >> 16);
			if (!layer_operation::point_in_header(mouse_x, mouse_y)) goto default_handler;

			// AviUtl must be in a suitable state.
			if (!fp->exfunc->is_editing(editp) || fp->exfunc->is_saving(editp)) goto default_handler;

			// initialize related variables.
			layer_prev = layer_operation::point_to_layer(mouse_y);

			if (curr_operation = dynamic_cast<layer_drag_operation const*>(cand_operation))
				// assigned operation is a drag. capture the mouse.
				::SetCapture(hwnd);

			return cand_operation->initialize(layer_prev, editp)
				&& cand_operation->notify() ? TRUE : FALSE;
		}

	default_handler:
		return exedit.func_wndproc(hwnd, message, wparam, lparam, editp, fp);
	}
};


////////////////////////////////
// AviUtlに渡す関数の定義．
////////////////////////////////
static BOOL func_init(AviUtl::FilterPlugin* fp)
{
	// 情報源確保．
	if (!exedit.init(fp)) {
		::MessageBoxA(nullptr, "拡張編集0.92が見つかりませんでした．",
			fp->name, MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}

	// 設定ロード．
	char path[MAX_PATH];
	auto len = ::GetModuleFileNameA(fp->dll_hinst, path, std::size(path));
	if (len >= 3) {
		::strcpy_s(&path[len - 3], 3 + 1, "ini");
		settings.load(path);
	}

	// コールバック関数差し替え．
	exedit.fp->func_WndProc = &Drag::func_wndproc_detour;

	return TRUE;
}


////////////////////////////////
// Entry point.
////////////////////////////////
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		::DisableThreadLibraryCalls(hinst);
		break;
	}
	return TRUE;
}


////////////////////////////////
// 看板．
////////////////////////////////
#define PLUGIN_NAME		"レイヤー一括切り替え"
#define PLUGIN_VERSION	"v1.40"
#define PLUGIN_AUTHOR	"sigma-axis"
#define PLUGIN_INFO_FMT(name, ver, author)	(name##" "##ver##" by "##author)
#define PLUGIN_INFO		PLUGIN_INFO_FMT(PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_AUTHOR)

extern "C" __declspec(dllexport) AviUtl::FilterPluginDLL* __stdcall GetFilterTable(void)
{
	// （フィルタとは名ばかりの）看板．
	using Flag = AviUtl::FilterPlugin::Flag;
	static constinit AviUtl::FilterPluginDLL filter{
		.flag = Flag::NoConfig | Flag::AlwaysActive | Flag::ExInformation,
		.name = PLUGIN_NAME,

		.func_init = func_init,
		.information = PLUGIN_INFO,
	};
	return &filter;
}
