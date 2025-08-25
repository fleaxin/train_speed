#include "global_logger.h"
#include <zlog.h>
#include <stdlib.h>

zlog_category_t *global_zlog_category = nullptr;  // 定义全局变量

class ZLogInitializer {
public:
    ZLogInitializer() {
        // 初始化zlog配置
        if (zlog_init("zlog.conf")) {
            fprintf(stderr, "zlog_init failed\n");
            exit(EXIT_FAILURE);
        }
        
        // 获取日志分类
        global_zlog_category = zlog_get_category("global");
        if (!global_zlog_category) {
            fprintf(stderr, "zlog_get_category failed\n");
            zlog_fini();
            exit(EXIT_FAILURE);
        }
    }

    ~ZLogInitializer() {
        zlog_fini();  // 程序退出时释放资源
    }
};

static ZLogInitializer initializer;  // 静态初始化触发构造函数