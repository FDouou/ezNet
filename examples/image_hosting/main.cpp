#include <iostream>
#include <csignal>
#include <sys/stat.h>
#include "core/EventLoop.h"
#include "core/TcpServer.h"
#include "http/HttpServer.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "util/Logger.h"
#include "ImageHosting.h"

using namespace ezNet;

static EventLoop* gLoop = nullptr;

void signalHandler(int) {
    LOG_INFO("Shutting down...");
    if (gLoop) gLoop->stop();
}

int main(int argc, char* argv[]) {
    int port = 8080;
    std::string storageDir = "/tmp/eznet_images";
    if (argc > 1) port = std::stoi(argv[1]);
    if (argc > 2) storageDir = argv[2];

    // 创建存储目录
    mkdir(storageDir.c_str(), 0755);

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    EventLoop loop;
    gLoop = &loop;

    TcpServer tcpServer(&loop, port);
    HttpServer httpServer(&tcpServer);

    ImageHosting imgHost(&loop, storageDir);

    // 上传接口
    httpServer.addRoute("POST", "/upload", [&](const HttpRequest& req, HttpResponse* resp,
                                                std::shared_ptr<Connection>) {
        imgHost.handleUpload(req, resp);
    });

    // 图片展示（利用 processRequest 对 resp.isFile() 的支持，自动走 sendfile）
    httpServer.addRoute("GET", "/img/:filename", [&](const HttpRequest& req, HttpResponse* resp,
                                                      std::shared_ptr<Connection>) {
        std::string filename = req.pathParam("filename");
        if (filename.empty() ||
            filename.find("..") != std::string::npos ||
            filename.find('/') != std::string::npos ||
            filename.find('\\') != std::string::npos) {
            resp->setStatusCode(400);
            resp->setBody("{\"error\":\"invalid filename\"}");
            return;
        }

        // 构建文件路径：/tmp/eznet_images/{prefix}/{filename}
        std::string prefix = filename.substr(0, 2);
        std::string filepath = storageDir + "/" + prefix + "/" + filename;

        struct stat st;
        if (stat(filepath.c_str(), &st) != 0) {
            resp->setStatusCode(404);
            resp->setBody("{\"error\":\"not found\"}");
            return;
        }

        // 设置文件响应，HttpServer::processRequest 会自动走 sendfile
        resp->setFile(filepath);
    });

    // 首页（简单的 HTML 上传表单）
    httpServer.addRoute("GET", "/", [](const HttpRequest&, HttpResponse* resp,
                                        std::shared_ptr<Connection>) {
        resp->setContentType("text/html; charset=utf-8");
        resp->setBody(
            "<!DOCTYPE html>"
            "<html><head><meta charset='utf-8'>"
            "<title>ezNet 图床</title>"
            "<style>"
            "body{font-family:sans-serif;max-width:800px;margin:0 auto;padding:20px;}"
            "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px;}"
            "form{border:2px dashed #ccc;padding:30px;text-align:center;border-radius:8px;"
            "  margin:20px 0;}"
            "form.dragover{border-color:#4CAF50;background:#f0fff0;}"
            "input[type=file]{margin:10px 0;}"
            "input[type=submit]{background:#4CAF50;color:white;padding:10px 30px;border:none;"
            "  border-radius:4px;cursor:pointer;font-size:16px;}"
            "input[type=submit]:hover{background:#45a049;}"
            "#result{margin-top:20px;padding:15px;background:#f5f5f5;border-radius:4px;"
            "  display:none;word-break:break-all;}"
            "#result img{max-width:100%;max-height:400px;margin-top:10px;border-radius:4px;"
            "  box-shadow:0 2px 8px rgba(0,0,0,0.1);}"
            ".url-box{background:#fff;padding:8px;border:1px solid #ddd;border-radius:3px;}"
            "</style></head><body>"
            "<h1>ezNet Image Hosting</h1>"
            "<form id='uploadForm'>"
            "<p>Drag & drop image or click to upload</p>"
            "<input type='file' id='fileInput' accept='image/*' required><br>"
            "<input type='submit' value='Upload'>"
            "</form>"
            "<div id='result'>"
            "<p><strong>URL:</strong></p>"
            "<div class='url-box'><a id='urlLink' href='' target='_blank'></a></div>"
            "<img id='preview' src='' alt='preview'>"
            "</div>"
            "<script>"
            "var form=document.getElementById('uploadForm');"
            "form.ondragover=function(e){e.preventDefault();"
            "  form.className='dragover';};"
            "form.ondragleave=function(){form.className='';};"
            "form.ondrop=function(e){e.preventDefault();form.className='';"
            "  document.getElementById('fileInput').files=e.dataTransfer.files;"
            "  form.dispatchEvent(new Event('submit'));};"
            "form.onsubmit=async function(e){"
            "  e.preventDefault();"
            "  var file=document.getElementById('fileInput').files[0];"
            "  if(!file)return;"
            "  var resp=await fetch('/upload',{method:'POST',body:file,"
            "    headers:{'Content-Type':file.type}});"
            "  var data=await resp.json();"
            "  var url='http://'+location.host+data.url;"
            "  document.getElementById('urlLink').textContent=url;"
            "  document.getElementById('urlLink').href=url;"
            "  document.getElementById('preview').src=url;"
            "  document.getElementById('result').style.display='block';"
            "};"
            "</script></body></html>"
        );
    });

    LOG_INFO("Image hosting started on port %d, storage: %s", port, storageDir.c_str());

    tcpServer.start();
    loop.loop();

    return 0;
}
