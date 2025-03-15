#pragma once
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <atomic>

// 结构体：存储文件读取结果
struct FileReadResult {
    std::string filename;
    std::shared_ptr<unsigned char[]> data;
    bool success;
};

// 线程安全任务队列
class FileReadManager {
private:
    std::queue<std::string> task_queue;  // 存储待处理的文件路径
    std::queue<FileReadResult> result_queue;  // 存储读取的结果
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> stop{false};  // 控制线程退出
    std::thread worker_thread;

public:
    FileReadManager() {
        worker_thread = std::thread(&FileReadManager::worker, this);
    }

    ~FileReadManager() {
        stopProcessing();
    }

    // 提交文件读取请求
    void addTask(const std::string& filename) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            task_queue.push(filename);
        }
        cv.notify_one();  // 通知工作线程有新任务
    }

    // 获取文件读取结果（如果有）
    bool getResult(FileReadResult& result) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (result_queue.empty()) {
            return false;
        }

        result = result_queue.front();
        result_queue.pop();
        return true;
    }

    // 停止后台线程
    void stopProcessing() {
        stop = true;
        cv.notify_all();
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }

private:
    // 后台线程函数：从队列中取任务，读取二进制文件
    void worker() {
        while (!stop) {
            std::string filename;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                cv.wait(lock, [this] { return !task_queue.empty() || stop; }); // 等待新任务
                if (stop) break; // 退出信号
                filename = task_queue.front();
                task_queue.pop();
            }

            // 读取二进制文件
            std::ifstream file(filename, std::ios::binary);
            FileReadResult result;
            result.filename = filename;
            result.success = false;

            if (file) {
                file.seekg(0, std::ios::end);
                size_t file_size = file.tellg();
                file.seekg(0, std::ios::beg);
                result.data.reset(new unsigned char[file_size]);
                file.read(reinterpret_cast<char*>(result.data.get()), file_size);
                result.success = file.good();
            }

            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                result_queue.push(result);
            }
        }
    }
};