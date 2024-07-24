/******************************************************************************
    Copyright (C) 2013-2014 by Hugh Bailey <obs.jim@gmail.com>

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

/**
 * @file
 * @brief header for modules implementing sources.
 *
 * Sources are modules that either feed data to libobs or modify it.
 */

#ifdef __cplusplus
extern "C" {
#endif

// 源类型
enum obs_source_type {
	OBS_SOURCE_TYPE_INPUT, // 输入源
	OBS_SOURCE_TYPE_FILTER, // 滤镜源
	OBS_SOURCE_TYPE_TRANSITION, // 转场源
	OBS_SOURCE_TYPE_SCENE, // 场景
};

// 音频相关，
enum obs_balance_type {
	OBS_BALANCE_TYPE_SINE_LAW, // 正弦
	OBS_BALANCE_TYPE_SQUARE_LAW, // 平方
	OBS_BALANCE_TYPE_LINEAR, // 线性
};

// 图标类型
enum obs_icon_type {
	OBS_ICON_TYPE_UNKNOWN,
	OBS_ICON_TYPE_IMAGE, // 图片
	OBS_ICON_TYPE_COLOR, // 颜色
	OBS_ICON_TYPE_SLIDESHOW, // 幻灯片
	OBS_ICON_TYPE_AUDIO_INPUT, // 音频输入
	OBS_ICON_TYPE_AUDIO_OUTPUT, // 音频输出
	OBS_ICON_TYPE_DESKTOP_CAPTURE, // 桌面捕获 
	OBS_ICON_TYPE_WINDOW_CAPTURE, // 窗口捕获 
	OBS_ICON_TYPE_GAME_CAPTURE, // 游戏捕获
	OBS_ICON_TYPE_CAMERA, // 摄像头
	OBS_ICON_TYPE_TEXT, // 文字源
	OBS_ICON_TYPE_MEDIA, // 多媒体源
	OBS_ICON_TYPE_BROWSER, // 浏览器源
	OBS_ICON_TYPE_CUSTOM, // 定制源
	OBS_ICON_TYPE_PROCESS_AUDIO_OUTPUT, // 处理音频输出
};

// 多媒体状态
enum obs_media_state {
	OBS_MEDIA_STATE_NONE,
	OBS_MEDIA_STATE_PLAYING, // 播放中
	OBS_MEDIA_STATE_OPENING, // 打开中
	OBS_MEDIA_STATE_BUFFERING, // 缓存中
	OBS_MEDIA_STATE_PAUSED, // 已暂停
	OBS_MEDIA_STATE_STOPPED, // 已停止
	OBS_MEDIA_STATE_ENDED, // 结束
	OBS_MEDIA_STATE_ERROR, // 报错
};

/**
 * @name Source output flags
 *
 * These flags determine what type of data the source outputs and expects.
 * @{
 */

/**
 * Source has video.
 *
 * Unless SOURCE_ASYNC_VIDEO is specified, the source must include the
 * video_render callback in the source definition structure.
 */
// 有视频数据
#define OBS_SOURCE_VIDEO (1 << 0)

/**
 * Source has audio.
 *
 * Use the obs_source_output_audio function to pass raw audio data, which will
 * be automatically converted and uploaded.  If used with SOURCE_ASYNC_VIDEO,
 * audio will automatically be synced up to the video output.
 */
// 有音频数据
#define OBS_SOURCE_AUDIO (1 << 1)

/** Async video flag (use OBS_SOURCE_ASYNC_VIDEO) */
// 异步视频标识
#define OBS_SOURCE_ASYNC (1 << 2)

/**
 * Source passes raw video data via RAM.
 *
 * Use the obs_source_output_video function to pass raw video data, which will
 * be automatically uploaded at the specified timestamp.
 *
 * If this flag is specified, it is not necessary to include the video_render
 * callback.  However, if you wish to use that function as well, you must call
 * obs_source_getframe to get the current frame data, and
 * obs_source_releaseframe to release the data when complete.
 */
// 通过 obs_source_getframe 获取数据，obs_source_releaseframe释放数据
#define OBS_SOURCE_ASYNC_VIDEO (OBS_SOURCE_ASYNC | OBS_SOURCE_VIDEO)

/**
 * Source uses custom drawing, rather than a default effect.
 *
 * If this flag is specified, the video_render callback will pass a NULL
 * effect, and effect-based filters will not use direct rendering.
 */
// 定制的绘制，而不是默认 effect 绘制
#define OBS_SOURCE_CUSTOM_DRAW (1 << 3)

/**
 * Source supports interaction.
 *
 * When this is used, the source will receive interaction events
 * if they provide the necessary callbacks in the source definition structure.
 */
// 支持交互
#define OBS_SOURCE_INTERACTION (1 << 5)

/**
 * Source composites sub-sources
 *
 * When used specifies that the source composites one or more sub-sources.
 * Sources that render sub-sources must implement the audio_render callback
 * in order to perform custom mixing of sub-sources.
 *
 * This capability flag is always set for transitions.
 */
// 转场时使用，该源有多个子源组成
#define OBS_SOURCE_COMPOSITE (1 << 6)

/**
 * Source should not be fully duplicated
 *
 * When this is used, specifies that the source should not be fully duplicated,
 * and should prefer to duplicate via holding references rather than full
 * duplication.
 */
// 通过引用而不是复制来创建复制源
#define OBS_SOURCE_DO_NOT_DUPLICATE (1 << 7)

/**
 * Source is deprecated and should not be used
 */
// 废弃
#define OBS_SOURCE_DEPRECATED (1 << 8)

/**
 * Source cannot have its audio monitored
 *
 * Specifies that this source may cause a feedback loop if audio is monitored
 * with a device selected as desktop audio.
 *
 * This is used primarily with desktop audio capture sources.
 */
// 此源不能监听，不然可能多次采集
#define OBS_SOURCE_DO_NOT_SELF_MONITOR (1 << 9)

/**
 * Source type is currently disabled and should not be shown to the user
 */
// 源失能，不能展示给用户
#define OBS_SOURCE_CAP_DISABLED (1 << 10)

/**
 * Source type is obsolete (has been updated with new defaults/properties/etc)
 */
// 过时的
#define OBS_SOURCE_CAP_OBSOLETE OBS_SOURCE_CAP_DISABLED

/**
 * Source should enable monitoring by default.  Monitoring should be set by the
 * frontend if this flag is set.
 */
// 默认监听
#define OBS_SOURCE_MONITOR_BY_DEFAULT (1 << 11)

/** Used internally for audio submixing */
// 内部用于音频混音
#define OBS_SOURCE_SUBMIX (1 << 12)

/**
 * Source type can be controlled by media controls
 */
// 源类型可以通过媒体控件进行控制
#define OBS_SOURCE_CONTROLLABLE_MEDIA (1 << 13)

/**
 * Source type provides cea708 data
 */
#define OBS_SOURCE_CEA_708 (1 << 14)

/**
 * Source understands SRGB rendering
 */
// 支持 srgb 渲染
#define OBS_SOURCE_SRGB (1 << 15)

/**
 * Source type prefers not to have its properties shown on creation
 * (prefers to rely on defaults first)
 */
// 源类型不希望在创建时显示其属性
#define OBS_SOURCE_CAP_DONT_SHOW_PROPERTIES (1 << 16)

/** @} */

typedef void (*obs_source_enum_proc_t)(obs_source_t *parent,
				       obs_source_t *child, void *param);

struct obs_source_audio_mix {
	struct audio_output_data output[MAX_AUDIO_MIXES];
};

/**
 * Source definition structure
 */
// 源结构体
struct obs_source_info {
	/* ----------------------------------------------------------------- */
	/* Required implementation*/

	/** Unique string identifier for the source */
	const char *id; // id 唯一标识

	/**
	 * Type of source.
	 *
	 * OBS_SOURCE_TYPE_INPUT for input sources,
	 * OBS_SOURCE_TYPE_FILTER for filter sources, and
	 * OBS_SOURCE_TYPE_TRANSITION for transition sources.
	 */
	// 输入源；滤镜；专场；场景都是 source
	enum obs_source_type type;

	/** Source output flags */
	// 输出标识
	uint32_t output_flags;

	/**
	 * Get the translated name of the source type
	 *
	 * @param  type_data  The type_data variable of this structure
	 * @return               The translated name of the source type
	 */
	// 获取源类型的翻译名称
	const char *(*get_name)(void *type_data);

	/**
	 * Creates the source data for the source
	 *
	 * @param  settings  Settings to initialize the source with
	 * @param  source    Source that this data is associated with
	 * @return           The data associated with this source
	 */
	// 创建源
	void *(*create)(obs_data_t *settings, obs_source_t *source);

	/**
	 * Destroys the private data for the source
	 *
	 * Async sources must not call obs_source_output_video after returning
	 * from destroy
	 */
	// 销毁源
	void (*destroy)(void *data);

	/** Returns the width of the source.  Required if this is an input
	 * source and has non-async video */
	// 获取宽高
	uint32_t (*get_width)(void *data);

	/** Returns the height of the source.  Required if this is an input
	 * source and has non-async video */
	uint32_t (*get_height)(void *data);

	// 可选实现
	/* ----------------------------------------------------------------- */
	/* Optional implementation */

	/**
	 * Gets the default settings for this source
	 *
	 * @param[out]  settings  Data to assign default settings to
	 * @deprecated            Use get_defaults2 if type_data is needed
	 */
	// 获取默认值
	void (*get_defaults)(obs_data_t *settings);

	/**
	 * Gets the property information of this source
	 *
	 * @return         The properties data
	 * @deprecated     Use get_properties2 if type_data is needed
	 */
	// 获取源的属性
	obs_properties_t *(*get_properties)(void *data);

	/**
	 * Updates the settings for this source
	 *
	 * @param data      Source data
	 * @param settings  New settings for this source
	 */
	// 更新源的设置
	void (*update)(void *data, obs_data_t *settings);

	/** Called when the source has been activated in the main view */
	// 启用
	void (*activate)(void *data);

	/**
	 * Called when the source has been deactivated from the main view
	 * (no longer being played/displayed)
	 */
	// 停用
	void (*deactivate)(void *data);

	/** Called when the source is visible */
	// 显示
	void (*show)(void *data);

	/** Called when the source is no longer visible */
	// 隐藏
	void (*hide)(void *data);

	/**
	 * Called each video frame with the time elapsed // 随着时间流逝，调用每个视频帧
	 *
	 * @param  data     Source data
	 * @param  seconds  Seconds elapsed since the last frame
	 */
	void (*video_tick)(void *data, float seconds);

	/**
	 * Called when rendering the source with the graphics subsystem.
	 *
	 * If this is an input/transition source, this is called to draw the
	 * source texture with the graphics subsystem using the specified
	 * effect.
	 *
	 * If this is a filter source, it wraps source draw calls (for
	 * example applying a custom effect with custom parameters to a
	 * source).  In this case, it's highly recommended to use the
	 * obs_source_process_filter function to automatically handle
	 * effect-based filter processing.  However, you can implement custom
	 * draw handling as desired as well.
	 *
	 * If the source output flags do not include SOURCE_CUSTOM_DRAW, all
	 * a source needs to do is set the "image" parameter of the effect to
	 * the desired texture, and then draw.  If the output flags include
	 * SOURCE_COLOR_MATRIX, you may optionally set the "color_matrix"
	 * parameter of the effect to a custom 4x4 conversion matrix (by
	 * default it will be set to an YUV->RGB conversion matrix)
	 *
	 * @param data    Source data
	 * @param effect  Effect to be used with this source.  If the source
	 *                output flags include SOURCE_CUSTOM_DRAW, this will
	 *                be NULL, and the source is expected to process with
	 *                an effect manually.
	 */
	// 视频渲染
	void (*video_render)(void *data, gs_effect_t *effect);

	/**
	 * Called to filter raw async video data.
	 *
	 * @note          This function is only used with filter sources.
	 *
	 * @param  data   Filter data
	 * @param  frame  Video frame to filter
	 * @return        New video frame data.  This can defer video data to
	 *                be drawn later if time is needed for processing
	 */
	// 对异步视频源做滤镜处理
	struct obs_source_frame *(*filter_video)(
		void *data, struct obs_source_frame *frame);

	/**
	 * Called to filter raw audio data.
	 *
	 * @note          This function is only used with filter sources.
	 *
	 * @param  data   Filter data
	 * @param  audio  Audio data to filter.
	 * @return        Modified or new audio data.  You can directly modify
	 *                the data passed and return it, or you can defer audio
	 *                data for later if time is needed for processing.  If
	 *                you are returning new data, that data must exist
	 *                until the next call to the filter_audio callback or
	 *                until the filter is removed/destroyed.
	 */
	// 对音频数据做滤镜处理
	struct obs_audio_data *(*filter_audio)(void *data,
					       struct obs_audio_data *audio);

	/**
	 * Called to enumerate all active sources being used within this
	 * source.  If the source has children that render audio/video it must
	 * implement this callback.
	 *
	 * @param  data           Filter data
	 * @param  enum_callback  Enumeration callback
	 * @param  param          User data to pass to callback
	 */
	// 枚举和当前源相关的且启用的子源
	void (*enum_active_sources)(void *data,
				    obs_source_enum_proc_t enum_callback,
				    void *param);

	/**
	 * Called when saving a source.  This is a separate function because
	 * sometimes a source needs to know when it is being saved so it
	 * doesn't always have to update the current settings until a certain
	 * point.
	 *
	 * @param  data      Source data
	 * @param  settings  Settings
	 */
	void (*save)(void *data, obs_data_t *settings);

	/**
	 * Called when loading a source from saved data.  This should be called
	 * after all the loading sources have actually been created because
	 * sometimes there are sources that depend on each other.
	 *
	 * @param  data      Source data
	 * @param  settings  Settings
	 */
	void (*load)(void *data, obs_data_t *settings);

	/**
	 * Called when interacting with a source and a mouse-down or mouse-up
	 * occurs.
	 *
	 * @param data         Source data
	 * @param event        Mouse event properties
	 * @param type         Mouse button pushed
	 * @param mouse_up     Mouse event type (true if mouse-up)
	 * @param click_count  Mouse click count (1 for single click, etc.)
	 */
	// 交互源
	void (*mouse_click)(void *data, const struct obs_mouse_event *event,
			    int32_t type, bool mouse_up, uint32_t click_count);
	/**
	 * Called when interacting with a source and a mouse-move occurs.
	 *
	 * @param data         Source data
	 * @param event        Mouse event properties
	 * @param mouse_leave  Mouse leave state (true if mouse left source)
	 */
	void (*mouse_move)(void *data, const struct obs_mouse_event *event,
			   bool mouse_leave);

	/**
	 * Called when interacting with a source and a mouse-wheel occurs.
	 *
	 * @param data         Source data
	 * @param event        Mouse event properties
	 * @param x_delta      Movement delta in the horizontal direction
	 * @param y_delta      Movement delta in the vertical direction
	 */
	void (*mouse_wheel)(void *data, const struct obs_mouse_event *event,
			    int x_delta, int y_delta);
	/**
	 * Called when interacting with a source and gain focus/lost focus event
	 * occurs.
	 *
	 * @param data         Source data
	 * @param focus        Focus state (true if focus gained)
	 */
	// 获取焦点或失去焦点
	void (*focus)(void *data, bool focus);

	/**
	 * Called when interacting with a source and a key-up or key-down
	 * occurs.
	 *
	 * @param data         Source data
	 * @param event        Key event properties
	 * @param focus        Key event type (true if mouse-up)
	 */
	void (*key_click)(void *data, const struct obs_key_event *event,
			  bool key_up);

	/**
	 * Called when the filter is removed from a source
	 *
	 * @param  data    Filter data
	 * @param  source  Source that the filter being removed from
	 */
	// 移除滤镜
	void (*filter_remove)(void *data, obs_source_t *source);

	/**
	 * Private data associated with this entry
	 */
	// 与此条目相关的私人数据
	void *type_data;

	/**
	 * If defined, called to free private data on shutdown
	 */
	// 释放私人数据
	void (*free_type_data)(void *type_data);

	// 音频渲染
	bool (*audio_render)(void *data, uint64_t *ts_out,
			     struct obs_source_audio_mix *audio_output,
			     uint32_t mixers, size_t channels,
			     size_t sample_rate);

	/**
	 * Called to enumerate all active and inactive sources being used
	 * within this source.  If this callback isn't implemented,
	 * enum_active_sources will be called instead.
	 *
	 * This is typically used if a source can have inactive child sources.
	 *
	 * @param  data           Filter data
	 * @param  enum_callback  Enumeration callback
	 * @param  param          User data to pass to callback
	 */
	// 枚举和当前源相关的子源
	void (*enum_all_sources)(void *data,
				 obs_source_enum_proc_t enum_callback,
				 void *param);
	// 转场开始结束
	void (*transition_start)(void *data);
	void (*transition_stop)(void *data);

	/**
	 * Gets the default settings for this source
	 * 
	 * If get_defaults is also defined both will be called, and the first
	 * call will be to get_defaults, then to get_defaults2.
	 *
	 * @param       type_data The type_data variable of this structure
	 * @param[out]  settings  Data to assign default settings to
	 */
	// 得到默认值
	void (*get_defaults2)(void *type_data, obs_data_t *settings);

	/**
	 * Gets the property information of this source
	 *
	 * @param data      Source data
	 * @param type_data The type_data variable of this structure
	 * @return          The properties data
	 */
	obs_properties_t *(*get_properties2)(void *data, void *type_data);

	// 混音
	bool (*audio_mix)(void *data, uint64_t *ts_out,
			  struct audio_output_data *audio_output,
			  size_t channels, size_t sample_rate);

	/** Icon type for the source */
	enum obs_icon_type icon_type; // 图标

	// 多媒体控制
	/** Media controls */
	void (*media_play_pause)(void *data, bool pause);
	void (*media_restart)(void *data);
	void (*media_stop)(void *data);
	void (*media_next)(void *data);
	void (*media_previous)(void *data);
	int64_t (*media_get_duration)(void *data);
	int64_t (*media_get_time)(void *data);
	void (*media_set_time)(void *data, int64_t miliseconds);
	enum obs_media_state (*media_get_state)(void *data);

	// 版本信息
	/* version-related stuff */
	uint32_t version; /* increment if needed to specify a new version */
	const char *unversioned_id; /* set internally, don't set manually */

	// 丢失文件
	/** Missing files **/
	obs_missing_files_t *(*missing_files)(void *data);

	// 获取颜色空间
	/** Get color space **/
	enum gs_color_space (*video_get_color_space)(
		void *data, size_t count,
		const enum gs_color_space *preferred_spaces);
};

EXPORT void obs_register_source_s(const struct obs_source_info *info,
				  size_t size);

// 注册源
/**
 * Registers a source definition to the current obs context.  This should be
 * used in obs_module_load.
 *
 * @param  info  Pointer to the source definition structure
 */
#define obs_register_source(info) \
	obs_register_source_s(info, sizeof(struct obs_source_info))

#ifdef __cplusplus
}
#endif
