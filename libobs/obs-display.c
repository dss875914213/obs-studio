/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "graphics/vec4.h"
#include "obs.h"
#include "obs-internal.h"

// 初始化预览窗口
bool obs_display_init(struct obs_display *display,
		      const struct gs_init_data *graphics_data)
{
	pthread_mutex_init_value(&display->draw_callbacks_mutex);
	pthread_mutex_init_value(&display->draw_info_mutex);

#if defined(_WIN32)
	/* Conservative test for NVIDIA flickering in multi-GPU setups */
	// 在多 GPU 设置中对 NVIDIA 闪烁进行保守测试
	display->use_clear_workaround = gs_get_adapter_count() > 1;
#elif defined(__APPLE__)
	/* Apple Silicon GL driver doesn't seem to track SRGB clears correctly */
	display->use_clear_workaround = true;
#else
	display->use_clear_workaround = false;
#endif

	if (graphics_data) {
		// 创建交换链
		display->swap = gs_swapchain_create(graphics_data);
		if (!display->swap) {
			blog(LOG_ERROR, "obs_display_init: Failed to "
					"create swap chain");
			return false;
		}

		const uint32_t cx = graphics_data->cx;
		const uint32_t cy = graphics_data->cy;
		display->cx = cx;
		display->cy = cy;
		display->next_cx = cx;
		display->next_cy = cy;
	}

	if (pthread_mutex_init(&display->draw_callbacks_mutex, NULL) != 0) {
		blog(LOG_ERROR, "obs_display_init: Failed to create mutex");
		return false;
	}
	if (pthread_mutex_init(&display->draw_info_mutex, NULL) != 0) {
		blog(LOG_ERROR, "obs_display_init: Failed to create mutex");
		return false;
	}

	display->enabled = true;
	return true;
}

// 创建预览窗口
obs_display_t *obs_display_create(const struct gs_init_data *graphics_data,
				  uint32_t background_color)
{
	struct obs_display *display = bzalloc(sizeof(struct obs_display));
	// dx11 不做任何事情
	gs_enter_context(obs->video.graphics);
	// 设置背景颜色
	display->background_color = background_color;

	if (!obs_display_init(display, graphics_data)) {
		obs_display_destroy(display);
		display = NULL;
	} else {
		pthread_mutex_lock(&obs->data.displays_mutex);
		// 每太特别看懂，反正是把 display 插到最前面
		display->prev_next = &obs->data.first_display;
		display->next = obs->data.first_display;
		obs->data.first_display = display;
		if (display->next)
			display->next->prev_next = &display->next;
		pthread_mutex_unlock(&obs->data.displays_mutex);
	}

	gs_leave_context();

	return display;
}

// 释放预览窗口
void obs_display_free(obs_display_t *display)
{
	pthread_mutex_destroy(&display->draw_callbacks_mutex);
	pthread_mutex_destroy(&display->draw_info_mutex);
	da_free(display->draw_callbacks);

	if (display->swap) {
		gs_swapchain_destroy(display->swap);
		display->swap = NULL;
	}
}

// 销毁预览窗口
void obs_display_destroy(obs_display_t *display)
{
	if (display) {
		pthread_mutex_lock(&obs->data.displays_mutex);
		if (display->prev_next)
			*display->prev_next = display->next;
		if (display->next)
			display->next->prev_next = display->prev_next;
		pthread_mutex_unlock(&obs->data.displays_mutex);

		obs_enter_graphics();
		obs_display_free(display);
		obs_leave_graphics();

		bfree(display);
	}
}

// 修改预览窗口大小
void obs_display_resize(obs_display_t *display, uint32_t cx, uint32_t cy)
{
	if (!display)
		return;

	pthread_mutex_lock(&display->draw_info_mutex);

	display->next_cx = cx;
	display->next_cy = cy;

	pthread_mutex_unlock(&display->draw_info_mutex);
}

// 更新颜色空间
void obs_display_update_color_space(obs_display_t *display)
{
	if (!display)
		return;

	pthread_mutex_lock(&display->draw_info_mutex);

	display->update_color_space = true;

	pthread_mutex_unlock(&display->draw_info_mutex);
}

// 增加/移除 绘制回调
void obs_display_add_draw_callback(obs_display_t *display,
				   void (*draw)(void *param, uint32_t cx,
						uint32_t cy),
				   void *param)
{
	if (!display)
		return;

	struct draw_callback data = {draw, param};

	pthread_mutex_lock(&display->draw_callbacks_mutex);
	da_push_back(display->draw_callbacks, &data);
	pthread_mutex_unlock(&display->draw_callbacks_mutex);
}

void obs_display_remove_draw_callback(obs_display_t *display,
				      void (*draw)(void *param, uint32_t cx,
						   uint32_t cy),
				      void *param)
{
	if (!display)
		return;

	struct draw_callback data = {draw, param};

	pthread_mutex_lock(&display->draw_callbacks_mutex);
	da_erase_item(display->draw_callbacks, &data);
	pthread_mutex_unlock(&display->draw_callbacks_mutex);
}

// 开始预览绘制
static inline bool render_display_begin(struct obs_display *display,
					uint32_t cx, uint32_t cy,
					bool update_color_space)
{
	struct vec4 clear_color;
	// 好像设置了渲染目标
	gs_load_swapchain(display->swap);

	if ((display->cx != cx) || (display->cy != cy)) {
		gs_resize(cx, cy);
		display->cx = cx;
		display->cy = cy;
	} else if (update_color_space) {
		gs_update_color_space();
	}
	// 是否可以渲染了
	const bool success = gs_is_present_ready();
	if (success) {
		// 重置纹理和颜色着色器资源
		gs_begin_scene();

		if (gs_get_color_space() == GS_CS_SRGB)
			vec4_from_rgba(&clear_color, display->background_color);
		else
			vec4_from_rgba_srgb(&clear_color,
					    display->background_color);
		clear_color.w = 1.0f;

		const bool use_clear_workaround = display->use_clear_workaround;

		uint32_t clear_flags = GS_CLEAR_DEPTH | GS_CLEAR_STENCIL;
		if (!use_clear_workaround)
			clear_flags |= GS_CLEAR_COLOR;
		//清理渲染目标 和 深度以及模板视图
		gs_clear(clear_flags, &clear_color, 1.0f, 0);
		// 关闭深度测试
		gs_enable_depth_test(false);
		/* gs_enable_blending(false); */
		// 设置光栅剔除模式  D3D11_CULL_NONE
		gs_set_cull_mode(GS_NEITHER);
		// 设置正交矩阵
		gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);
		// 设置视口
		gs_set_viewport(0, 0, cx, cy);

		// 没太看懂，没有设置纹理视图，这边是在绘制什么，只是清空背景吗？
		if (use_clear_workaround) {
			gs_effect_t *const solid_effect =
				obs->video.solid_effect;
			gs_effect_set_vec4(gs_effect_get_param_by_name(
						   solid_effect, "color"),
					   &clear_color);
			while (gs_effect_loop(solid_effect, "Solid"))
				gs_draw_sprite(NULL, 0, cx, cy);
		}
	}

	return success;
}

static inline void render_display_end()
{
	// dx11 啥都没干
	gs_end_scene();
}

// 预览窗口开始渲染
void render_display(struct obs_display *display)
{
	uint32_t cx, cy;
	bool update_color_space;

	if (!display || !display->enabled)
		return;

	// 获取当前宽高和是否更新颜色空间
	/* -------------------------------------------- */

	pthread_mutex_lock(&display->draw_info_mutex);

	cx = display->next_cx;
	cy = display->next_cy;
	update_color_space = display->update_color_space;

	display->update_color_space = false;

	pthread_mutex_unlock(&display->draw_info_mutex);

	/* -------------------------------------------- */
	// 开始预览绘制
	if (render_display_begin(display, cx, cy, update_color_space)) {
		GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_DISPLAY, "obs_display");

		pthread_mutex_lock(&display->draw_callbacks_mutex);

		for (size_t i = 0; i < display->draw_callbacks.num; i++) {
			struct draw_callback *callback;
			callback = display->draw_callbacks.array + i;

			callback->draw(callback->param, cx, cy);
		}

		pthread_mutex_unlock(&display->draw_callbacks_mutex);

		render_display_end();

		GS_DEBUG_MARKER_END();

		gs_present();
	}
}

void obs_display_set_enabled(obs_display_t *display, bool enable)
{
	if (display)
		display->enabled = enable;
}

// 初始化成功
bool obs_display_enabled(obs_display_t *display)
{
	return display ? display->enabled : false;
}

// 设置背景颜色
void obs_display_set_background_color(obs_display_t *display, uint32_t color)
{
	if (display)
		display->background_color = color;
}

// 获取预览窗口宽高
void obs_display_size(obs_display_t *display, uint32_t *width, uint32_t *height)
{
	*width = 0;
	*height = 0;

	if (display) {
		pthread_mutex_lock(&display->draw_info_mutex);

		*width = display->cx;
		*height = display->cy;

		pthread_mutex_unlock(&display->draw_info_mutex);
	}
}
