// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include "match_server/Match.h"
#include "save_client/Save.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/transport/TSocket.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/ThreadFactory.h>
#include <thrift/TToString.h>
#include <thrift/server/TThreadedServer.h>

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace ::match_server;
using namespace ::save_service;
using namespace std;

struct Task
{
    User user;
    string type;
};//定义任务类型

struct MessageQueue
{
    queue<Task> q;
    mutex m;
    condition_variable cv;
} message_queue;//自我实现消息队列

class Pool
{
    public:

        void save_result(int a, int b)
        {
            cout << a << " " << b << endl;

            std::shared_ptr<TTransport> socket(new TSocket("123.57.47.211", 9090));
            std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
            std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
            SaveClient client(protocol);

            try {
                transport->open();

                bool res = client.save_data("acs_2202", "21caf915", a, b);

                if (!res) cout << "success" << endl;
                else cout << "failed" << endl;

                transport->close();
            } catch (TException& tx) {
                cout << "ERROR: " << tx.what() << endl;
            }

        }


        bool check_match(uint32_t i, uint32_t j)
        {
            auto a = users[i], b = users[j];
            int dt = abs(a.score - b.score);

            if (wt[i] * 50 >= dt && wt[j] * 50 >= dt) return true;
            return false;
        }


        void match()
        {
            for (int& v: wt) v += 1;//等待秒数+1

            while (users.size() > 1)
            {
                bool flag = false;
                for (uint32_t i = 0; i < users.size(); i ++ )
                {
                    for (uint32_t j = i + 1; j < users.size(); j ++ )
                    {
                        if (check_match(i, j))
                        {
                            save_result(users[i].userId, users[j].userId);
                            users.erase(users.begin() + j);
                            users.erase(users.begin() + i);
                            wt.erase(wt.begin() + j);
                            wt.erase(wt.begin() + i);
                            flag = true;
                            break;
                        }
                    }

                    if (flag) break;
                }

                if (!flag) break;

            }
        }

        void add(User user)
        {
            users.push_back(user);
            wt.push_back(0);
        }


        void remove(User user)
        {
            for (uint32_t i = 0; i < users.size(); i ++ ) //防止报警告，使用无符号int
            {
                if (users[i].userId == user.userId)
                {
                    users.erase(users.begin() + i);
                    wt.erase(wt.begin() + i);
                    break;
                }
            }
        }

    private:
        vector<User> users;
        vector<int> wt;//等待秒数，单位:s
} pool;

class MatchHandler : virtual public MatchIf {
    public:
        MatchHandler() {
            // Your initialization goes here
        }

        int32_t add_user(const User& user, const std::string& info) {
            // Your implementation goes here
            printf("add_user\n");

            unique_lock<mutex> lck(message_queue.m);//生成一个锁，函数执行结束的时候，自动解锁
            message_queue.q.push({user, "add"});
            message_queue.cv.notify_all();//唤醒所有条件变量，所有被堵住的进程将会被唤醒
            //message_queue.cv.notify_one();唤醒一个进程

            return 0;
        }

        int32_t remove_user(const User& user, const std::string& info) {
            // Your implementation goes here
            printf("remove_user\n");

            unique_lock<mutex> lck(message_queue.m);//生成一个锁，函数执行结束的时候，自动解锁
            message_queue.q.push({user, "remove"});
            message_queue.cv.notify_all();//唤醒所有条件变量，所有被堵住的进程将会被唤醒

            return 0;
        }

};

class MatchCloneFactory : virtual public MatchIfFactory {
    public:
        ~MatchCloneFactory() override = default;
        MatchIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) override
        {
            std::shared_ptr<TSocket> sock = std::dynamic_pointer_cast<TSocket>(connInfo.transport);
            /*cout << "Incoming connection\n";
            cout << "\tSocketInfo: "  << sock->getSocketInfo() << "\n";
            cout << "\tPeerHost: "    << sock->getPeerHost() << "\n";
            cout << "\tPeerAddress: " << sock->getPeerAddress() << "\n";
            cout << "\tPeerPort: "    << sock->getPeerPort() << "\n";
            */
            return new MatchHandler;
        }
        void releaseHandler(MatchIf* handler) override {
            delete handler;
        }
};

void consume_task()
{
    while (true)
    {
        unique_lock<mutex> lck(message_queue.m);//占据锁
        if (message_queue.q.empty())
        {
            //队列为空的时候应该卡住进程，防止死循环。
            //message_queue.cv.wait(lck);//将锁释放掉，然后卡住进程，等到某个地方将条件变量cv唤醒才会继续往下只执行
            lck.unlock();//直接解锁
            pool.match();
            sleep(1);
            //continue;
        }
        else
        {
            auto task = message_queue.q.front();
            message_queue.q.pop();

            //处理task
            lck.unlock();//释放掉这个锁，防止持有锁的时间太长了，用完了记得及时解锁

            if (task.type == "add")
            {
                pool.add(task.user);
            }
            else if (task.type == "remove")
            {
                pool.remove(task.user);
            }

        }
    }
}


int main(int argc, char **argv) {
    TThreadedServer server(
            std::make_shared<MatchProcessorFactory>(std::make_shared<MatchCloneFactory>()),
            std::make_shared<TServerSocket>(9090), //port
            std::make_shared<TBufferedTransportFactory>(),
            std::make_shared<TBinaryProtocolFactory>());

    cout << "Start Match Server" << endl;

    thread matching_thread(consume_task);

    server.serve();
    return 0;
}

