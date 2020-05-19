#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include "ThreadPool.h"


void TestInt(int num) {
  std::this_thread::sleep_for(std::chrono::seconds(1));
  printf("TestInt输出数字：%d\n", num);
}

void TestString(std::string name) {
  printf("TestString输出字符串：%s\n", name.c_str());
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int TestReturn(int num) {
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  return num + 10;
}

int main() {
  setbuf(stdout, nullptr);
  ThreadPool myThreadPool(3, 8, 1000, 100);

  for (int i = 0; i < 60; i++) {
    myThreadPool.AddTask(TestInt, i);
  }

  std::this_thread::sleep_for(std::chrono::seconds(10));

  for (int i = 0; i < 60; i++) {
    myThreadPool.AddTask(TestString, std::to_string(i));
  }

  std::vector<std::future<int>> futures;
  for (int i = 0; i < 60; i++) {
    std::future<int> result = myThreadPool.AddTaskAndResult(TestReturn, i);
    futures.push_back(std::move(result));
  }

  for (auto &v : futures) {
    printf("%d", v.get());
  }

  myThreadPool.Wait();
  myThreadPool.AddTask(TestString,std::to_string(1));

  return 0;
}

