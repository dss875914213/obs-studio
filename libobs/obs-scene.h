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

#pragma once

#include "obs.h"
#include "graphics/matrix4.h"

/* how obs scene! */

struct item_action {
	bool visible;
	uint64_t timestamp;
};

struct obs_scene_item {
	volatile long ref;
	volatile bool removed;

	bool is_group; // 是否组合
	bool update_transform; // 更新几何参数
	bool update_group_resize;

	int64_t id;

	struct obs_scene *parent;
	struct obs_source *source; // 源
	volatile long active_refs; // 激活的索引次数
	volatile long defer_update; // 推迟更新
	volatile long defer_group_resize;
	bool user_visible; // 用户是否可见
	bool visible; // 是否可见
	bool selected; // 是否选择
	bool locked; // 是否锁定

	gs_texrender_t *item_render; // 渲染纹理
	struct obs_sceneitem_crop crop; // 裁剪

	struct vec2 pos; // 偏移量
	struct vec2 scale; // 缩放系数
	float rot; // 旋转角度
	uint32_t align; // 对齐

	/* last width/height of the source, this is used to check whether
	 * the transform needs updating */
	uint32_t last_width; // 上一次的宽高
	uint32_t last_height;

	struct vec2 output_scale;
	enum obs_scale_type scale_filter;

	enum obs_blending_method blend_method; // 混合方式
	enum obs_blending_type blend_type; // 混合类型

	struct matrix4 box_transform;
	struct vec2 box_scale;
	struct matrix4 draw_transform; // 几何参数矩阵

	enum obs_bounds_type bounds_type;
	uint32_t bounds_align;
	struct vec2 bounds;

	obs_hotkey_pair_id toggle_visibility; // 切换可见性

	obs_data_t *private_settings;

	pthread_mutex_t actions_mutex;
	DARRAY(struct item_action) audio_actions;

	struct obs_source *show_transition;
	struct obs_source *hide_transition;
	uint32_t show_transition_duration;
	uint32_t hide_transition_duration;

	/* would do **prev_next, but not really great for reordering */
	struct obs_scene_item *prev;
	struct obs_scene_item *next;
};

struct obs_scene {
	struct obs_source *source;

	bool is_group;
	bool custom_size;
	uint32_t cx;
	uint32_t cy;

	int64_t id_counter;

	pthread_mutex_t video_mutex;
	pthread_mutex_t audio_mutex;
	struct obs_scene_item *first_item; // 第一项
};
