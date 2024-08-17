#include <obs-module.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <sys/stat.h>

#define blog(log_level, format, ...)                    \
	blog(log_level, "[image_source: '%s'] " format, \
	     obs_source_get_name(context->source), ##__VA_ARGS__)

#define debug(format, ...) blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)

struct image_source {
	obs_source_t *source;

	char *file; // 文件名
	bool persistent; // 持久的，不使用时不卸载图片
	bool linear_alpha; // 在线性空间应用透明通道
	time_t file_timestamp; // 文件修改时间，当 tick 中检测到修改时间变了会重新加载
	float update_time_elapsed; // 统计检测文件修改的时间间隔，1s检测一次
	uint64_t last_time; // 上一次 tick 时间
	bool active; // 激活状态
	bool restart_gif;

	gs_image_file4_t if4;
};

// 获取文件更新时间
static time_t get_modified_timestamp(const char *filename)
{
	struct stat stats;
	if (os_stat(filename, &stats) != 0)
		return -1;
	return stats.st_mtime;
}

// 返回源名称
static const char *image_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("ImageInput");
}

static void image_source_load(struct image_source *context)
{
	char *file = context->file;

	obs_enter_graphics();
	gs_image_file4_free(&context->if4);
	obs_leave_graphics();

	if (file && *file) {
		debug("loading texture '%s'", file);
		context->file_timestamp = get_modified_timestamp(file);
		gs_image_file4_init(&context->if4, file,
				    context->linear_alpha
					    ? GS_IMAGE_ALPHA_PREMULTIPLY_SRGB
					    : GS_IMAGE_ALPHA_PREMULTIPLY);
		context->update_time_elapsed = 0;

		obs_enter_graphics();
		gs_image_file4_init_texture(&context->if4);
		obs_leave_graphics();

		if (!context->if4.image3.image2.image.loaded)
			warn("failed to load texture '%s'", file);
	}
}

static void image_source_unload(struct image_source *context)
{
	obs_enter_graphics();
	gs_image_file4_free(&context->if4);
	obs_leave_graphics();
}

// 解析 obs_data 中的数据，更新参数
static void image_source_update(void *data, obs_data_t *settings)
{
	struct image_source *context = data;
	const char *file = obs_data_get_string(settings, "file");
	const bool unload = obs_data_get_bool(settings, "unload");
	const bool linear_alpha = obs_data_get_bool(settings, "linear_alpha");

	if (context->file)
		bfree(context->file);
	context->file = bstrdup(file);
	context->persistent = !unload;
	context->linear_alpha = linear_alpha;

	/* Load the image if the source is persistent or showing */
	if (context->persistent || obs_source_showing(context->source))
		image_source_load(data);
	else
		image_source_unload(data);
}

// 获取默认值
static void image_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "unload", false);
	obs_data_set_default_bool(settings, "linear_alpha", false);
}

// 显示图片
static void image_source_show(void *data)
{
	struct image_source *context = data;

	if (!context->persistent)
		image_source_load(context);
}

// 隐藏图片
static void image_source_hide(void *data)
{
	struct image_source *context = data;

	if (!context->persistent)
		image_source_unload(context);
}

static void restart_gif(void *data)
{
	struct image_source *context = data;

	if (context->if4.image3.image2.image.is_animated_gif) {
		context->if4.image3.image2.image.cur_frame = 0;
		context->if4.image3.image2.image.cur_loop = 0;
		context->if4.image3.image2.image.cur_time = 0;

		obs_enter_graphics();
		gs_image_file4_update_texture(&context->if4);
		obs_leave_graphics();

		context->restart_gif = false;
	}
}

static void image_source_activate(void *data)
{
	struct image_source *context = data;
	context->restart_gif = true;
}

// 源创建
static void *image_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct image_source *context = bzalloc(sizeof(struct image_source));
	context->source = source;

	image_source_update(context, settings);
	return context;
}

// 源销毁
static void image_source_destroy(void *data)
{
	struct image_source *context = data;

	image_source_unload(context);

	if (context->file)
		bfree(context->file);
	bfree(context);
}

// 获取素材宽
static uint32_t image_source_getwidth(void *data)
{
	struct image_source *context = data;
	return context->if4.image3.image2.image.cx;
}

// 获取素材高
static uint32_t image_source_getheight(void *data)
{
	struct image_source *context = data;
	return context->if4.image3.image2.image.cy;
}

// 素材渲染
static void image_source_render(void *data, gs_effect_t *effect)
{
	struct image_source *context = data;

	struct gs_image_file *const image = &context->if4.image3.image2.image;
	gs_texture_t *const texture = image->texture;
	if (!texture)
		return;

	// 保存之前的 srgb 状态
	const bool previous = gs_framebuffer_srgb_enabled();
	// 开启 srgb
	gs_enable_framebuffer_srgb(true);
	// 保存之前的混合状态
	gs_blend_state_push();
	// 设置新的混合状态
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	// image 不是表示图片特有的 effect, 而是一类渲染方式，但是在哪设置的还没有找到
	gs_eparam_t *const param = gs_effect_get_param_by_name(effect, "image");
	// 设置 effect 纹理
	gs_effect_set_texture_srgb(param, texture);
	// 绘制图形
	gs_draw_sprite(texture, 0, image->cx, image->cy);
	// 恢复之前的混合状态
	gs_blend_state_pop();
	// 恢复之前的 srgb 状态
	gs_enable_framebuffer_srgb(previous);
}

static void image_source_tick(void *data, float seconds)
{
	// 把上下文传进来，上下文状态都是保存在 obs_core 里面的
	struct image_source *context = data;
	uint64_t frame_time = obs_get_video_frame_time();

	context->update_time_elapsed += seconds;

	// 只有需要渲染时才更新
	if (obs_source_showing(context->source)) {
		if (context->update_time_elapsed >= 1.0f) {
			time_t t = get_modified_timestamp(context->file);
			context->update_time_elapsed = 0.0f;

			if (context->file_timestamp != t) {
				image_source_load(context);
			}
		}
	}

	if (obs_source_showing(context->source)) {
		if (!context->active) {
			if (context->if4.image3.image2.image.is_animated_gif)
				context->last_time = frame_time;
			context->active = true;
		}

		if (context->restart_gif)
			restart_gif(context);

	} else {
		if (context->active) {
			restart_gif(context);
			context->active = false;
		}

		return;
	}

	// 如果是 gif 则更新纹理，纹理保存在 context->if4.image3.image2.image
	if (context->last_time &&
	    context->if4.image3.image2.image.is_animated_gif) {
		uint64_t elapsed = frame_time - context->last_time;
		bool updated = gs_image_file4_tick(&context->if4, elapsed);

		if (updated) {
			obs_enter_graphics();
			gs_image_file4_update_texture(&context->if4);
			obs_leave_graphics();
		}
	}

	context->last_time = frame_time;
}

static const char *image_filter =
#ifdef _WIN32
	"All formats (*.bmp *.tga *.png *.jpeg *.jpg *.jxr *.gif *.psd *.webp);;"
#else
	"All formats (*.bmp *.tga *.png *.jpeg *.jpg *.gif *.psd *.webp);;"
#endif
	"BMP Files (*.bmp);;"
	"Targa Files (*.tga);;"
	"PNG Files (*.png);;"
	"JPEG Files (*.jpeg *.jpg);;"
#ifdef _WIN32
	"JXR Files (*.jxr);;"
#endif
	"GIF Files (*.gif);;"
	"PSD Files (*.psd);;"
	"WebP Files (*.webp);;"
	"All Files (*.*)";
// 获取素材属性，感觉用来绘制界面用
static obs_properties_t *image_source_properties(void *data)
{
	struct image_source *s = data;
	struct dstr path = {0};

	obs_properties_t *props = obs_properties_create();

	if (s && s->file && *s->file) {
		const char *slash;

		dstr_copy(&path, s->file);
		dstr_replace(&path, "\\", "/");
		slash = strrchr(path.array, '/');
		if (slash)
			dstr_resize(&path, slash - path.array + 1);
	}

	obs_properties_add_path(props, "file", obs_module_text("File"),
				OBS_PATH_FILE, image_filter, path.array);
	obs_properties_add_bool(props, "unload",
				obs_module_text("UnloadWhenNotShowing"));
	obs_properties_add_bool(props, "linear_alpha",
				obs_module_text("LinearAlpha"));
	dstr_free(&path);

	return props;
}

// 幻灯片源使用
uint64_t image_source_get_memory_usage(void *data)
{
	struct image_source *s = data;
	return s->if4.image3.image2.mem_usage;
}

static void missing_file_callback(void *src, const char *new_path, void *data)
{
	struct image_source *s = src;

	obs_source_t *source = s->source;
	obs_data_t *settings = obs_source_get_settings(source);
	obs_data_set_string(settings, "file", new_path);
	obs_source_update(source, settings);
	obs_data_release(settings);

	UNUSED_PARAMETER(data);
}

// 统计缺少文件
static obs_missing_files_t *image_source_missingfiles(void *data)
{
	struct image_source *s = data;
	obs_missing_files_t *files = obs_missing_files_create();

	if (strcmp(s->file, "") != 0) {
		if (!os_file_exists(s->file)) {
			obs_missing_file_t *file = obs_missing_file_create(
				s->file, missing_file_callback,
				OBS_MISSING_FILE_SOURCE, s->source, NULL);

			obs_missing_files_add_file(files, file);
		}
	}

	return files;
}

// 返回颜色空间
static enum gs_color_space
image_source_get_color_space(void *data, size_t count,
			     const enum gs_color_space *preferred_spaces)
{
	UNUSED_PARAMETER(count);
	UNUSED_PARAMETER(preferred_spaces);

	struct image_source *const s = data;
	gs_image_file4_t *const if4 = &s->if4;
	return if4->image3.image2.image.texture ? if4->space : GS_CS_SRGB;
}

static struct obs_source_info image_source_info = {
	.id = "image_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB,
	.get_name = image_source_get_name,
	.create = image_source_create,
	.destroy = image_source_destroy,
	.update = image_source_update,
	.get_defaults = image_source_defaults,
	.show = image_source_show,
	.hide = image_source_hide,
	.get_width = image_source_getwidth,
	.get_height = image_source_getheight,
	.video_render = image_source_render,
	.video_tick = image_source_tick,
	.missing_files = image_source_missingfiles,
	.get_properties = image_source_properties,
	.icon_type = OBS_ICON_TYPE_IMAGE,
	.activate = image_source_activate,
	.video_get_color_space = image_source_get_color_space,
};

// 模型描述
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("image-source", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Image/color/slideshow sources";
}

extern struct obs_source_info slideshow_info;
extern struct obs_source_info color_source_info_v1;
extern struct obs_source_info color_source_info_v2;
extern struct obs_source_info color_source_info_v3;

// 模型加载
bool obs_module_load(void)
{
	// 注册源
	obs_register_source(&image_source_info);
	obs_register_source(&color_source_info_v1);
	obs_register_source(&color_source_info_v2);
	obs_register_source(&color_source_info_v3);
	obs_register_source(&slideshow_info);
	return true;
}
