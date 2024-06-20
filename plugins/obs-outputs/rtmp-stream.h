#include <obs-module.h>
#include <obs-avc.h>
#include <util/platform.h>
#include <util/circlebuf.h>
#include <util/dstr.h>
#include <util/threading.h>
#include <inttypes.h>
#include "librtmp/rtmp.h"
#include "librtmp/log.h"
#include "flv-mux.h"
#include "net-if.h"

#ifdef _WIN32
#include <Iphlpapi.h>
#else
#include <sys/ioctl.h>
#endif

#define do_log(level, format, ...)                 \
	blog(level, "[rtmp stream: '%s'] " format, \
	     obs_output_get_name(stream->output), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

#define OPT_DYN_BITRATE "dyn_bitrate"
#define OPT_DROP_THRESHOLD "drop_threshold_ms"
#define OPT_PFRAME_DROP_THRESHOLD "pframe_drop_threshold_ms"
#define OPT_MAX_SHUTDOWN_TIME_SEC "max_shutdown_time_sec"
#define OPT_BIND_IP "bind_ip"
#define OPT_NEWSOCKETLOOP_ENABLED "new_socket_loop_enabled"
#define OPT_LOWLATENCY_ENABLED "low_latency_mode_enabled"
#define OPT_METADATA_MULTITRACK "metadata_multitrack"

//#define TEST_FRAMEDROPS
//#define TEST_FRAMEDROPS_WITH_BITRATE_SHORTCUTS

#ifdef TEST_FRAMEDROPS

#define DROPTEST_MAX_KBPS 3000
#define DROPTEST_MAX_BYTES (DROPTEST_MAX_KBPS * 1000 / 8)

struct droptest_info {
	uint64_t ts;
	size_t size;
};
#endif

struct dbr_frame {
	uint64_t send_beg; // 开始发送时间
	uint64_t send_end; // 发送结束时间
	size_t size; // 发送包大小
};

struct rtmp_stream {
	obs_output_t *output;

	pthread_mutex_t packets_mutex; // 包的 mutex 锁
	struct circlebuf packets; // 待发送的包
	bool sent_headers; // 已经发送 header 信息

	bool got_first_video; // 是否收到第一个编码后的视频帧
	int64_t start_dts_offset; // 第一帧视频的 dts

	volatile bool connecting; // 正在重连标识
	pthread_t connect_thread; // 重连线程

	volatile bool active; // 激活状态
	volatile bool disconnected; // 断开连接
	volatile bool encode_error; // 如果是空包，则认为编码错误
	volatile bool silent_reconnect; // 收到服务端发送的重连消息，则进行静默重连逻辑
	pthread_t send_thread; // 发送线程

	int max_shutdown_time_sec; // 最大的关闭时间

	os_sem_t *send_sem; // 发送信号量
	os_event_t *stop_event; // 停止事件
	uint64_t stop_ts; // 停止事件
	uint64_t shutdown_timeout_ts; // 关闭超时时间

	struct dstr path, key; // 推流地址和 key
	struct dstr username, password; // 用户名和密码
	struct dstr encoder_name; // 编码器名字
	struct dstr bind_ip; // 设置推流 ip

	/* frame drop variables */
	int64_t drop_threshold_usec; // 丢b帧阈值
	int64_t pframe_drop_threshold_usec; // 丢p帧阈值
	int min_priority; // 放入数据队列的最小优先级
	float congestion; // 拥塞情况

	int64_t last_dts_usec; // 最新视频数据时间戳 

	uint64_t total_bytes_sent; //  总的发送字节数
	int dropped_frames; // 总丢帧数

#ifdef TEST_FRAMEDROPS
	struct circlebuf droptest_info;
	uint64_t droptest_last_key_check;
	size_t droptest_max;
	size_t droptest_size;
#endif

	// 检测网络环境，当网络差会调整编码码率
	// dbr 可能是 dynamic bitrate
	pthread_mutex_t dbr_mutex;
	struct circlebuf dbr_frames; // dbr 数据队列
	size_t dbr_data_size; // dbr 队列总字节数
	uint64_t dbr_inc_timeout; // 超时时间，当码率降低之后会重置，当码率上升之后且没有达到原始值，也重置
	long audio_bitrate; // 音频码率；预估出来的是总的码率，需要减去音频的码率，才是视频的码率
	long dbr_est_bitrate;
	long dbr_orig_bitrate; // 原始码率
	long dbr_prev_bitrate; // 上一次的码率
	long dbr_cur_bitrate; // 当前码率
	long dbr_inc_bitrate; // 每次增加多少，目前是原始码率的 1/10
	bool dbr_enabled; // 是否开启 dbr 检测 

	RTMP rtmp;

	bool new_socket_loop;
	bool low_latency_mode; // 低延时模式
	bool disable_send_window_optimization; // 没有地方设置，关闭发送窗口优化
	bool socket_thread_active; // 没有地方使用，
	pthread_t socket_thread; // socket 线程，不知道里面在干啥
	uint8_t *write_buf; // 好像是 rtmp-windows 文件里面使用，不知道里面是干啥的
	size_t write_buf_len;
	size_t write_buf_size;
	pthread_mutex_t write_buf_mutex;
	os_event_t *buffer_space_available_event;
	os_event_t *buffer_has_data_event;
	os_event_t *socket_available_event;
	os_event_t *send_thread_signaled_exit;
};

#ifdef _WIN32
void *socket_thread_windows(void *data);
#endif
