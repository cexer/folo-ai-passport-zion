#include "zion_text.h"
#include <stdio.h>
#include <string.h>

const char *zion_text_zh(const char *message)
{
    static const struct { const char *wire; const char *zh; } words[] = {
        {"WiFi setup AP ready.", "配网热点已就绪"},
        {"Connecting to WiFi...", "正在连接 Wi-Fi"},
        {"Connecting...", "正在连接"},
        {"Online.", "已联网"},
        {"Setup AP failed.", "配网热点启动失败"},
        {"Reset stuck, retry.", "重置未完成，请重试"},
        {"Wrong password?", "请检查密码"},
        {"AP not found. Re-scan.", "未找到热点，请重新扫描"},
        {"Auth failed: password/PMF?", "认证失败，请检查密码或 PMF"},
        {"Handshake timeout.", "连接握手超时"},
        {"Router refused (PMF/RSN).", "路由器拒绝连接（PMF/RSN）"},
        {"WiFi connect failed.", "Wi-Fi 连接失败"},
        {"scan QR or enter pair code", "请扫码或输入配对码"},
        {"waiting for phone upload", "等待手机上传"},
        {"offer declined on device", "已在设备上拒绝安装"},
        {"upload session ready", "上传已就绪"},
        {"uploading", "正在上传"},
        {"installed", "安装完成"},
        {"cancelled", "已取消"},
        {"slot carved on phone pick", "已按手机选择分配空间"},
        {"slot chosen on phone", "已在手机选择槽位"},
        {"confirm on device", "请在设备上确认"},
        {"partition missing", "分区层：未找到分区"},
        {"flash begin failed", "Flash 层：写入初始化失败"},
        {"flash write failed", "Flash 层：写入失败"},
        {"data erase failed", "数据层：擦除失败"},
        {"data write failed", "数据层：写入失败"},
        {"data upload incomplete", "数据层：上传不完整"},
        {"data verify failed", "数据层：校验失败"},
        {"length mismatch", "传输层：长度不匹配"},
        {"checksum mismatch", "传输层：校验值不匹配"},
        {"no ota session", "OTA 层：会话未建立"},
        {"verify failed", "OTA 层：校验失败"},
        {"image verify failed", "镜像层：校验失败"},
        {"image length mismatch", "镜像层：长度不匹配"},
        {"name write failed", "名称层：写入失败"},
        {"carve state update failed", "分配层：状态更新失败"},
        {"registry write failed", "登记层：写入失败"},
    };
    if (!message) return "";
    for (unsigned i = 0; i < sizeof(words) / sizeof(words[0]); ++i)
        if (strcmp(message, words[i].wire) == 0) return words[i].zh;
    static char code_text[80];
    int code;
    if (sscanf(message, "WiFi failed (code %d).", &code) == 1) {
        snprintf(code_text, sizeof(code_text), "Wi-Fi 连接失败（代码 %d）", code);
        return code_text;
    }
    return message; /* Unknown machine reason codes are retained for diagnosis. */
}
