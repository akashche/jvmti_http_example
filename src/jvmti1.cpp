/* 
 * File:   jvmti1.cpp
 * Author: alex
 * 
 * Created on May 5, 2015, 1:09 PM
 */

#include <iostream>
#include <string>
#include <mutex>
#include <chrono>

#include <jvmti.h>

#include "asio.hpp"

#include "pion/tcp/connection.hpp"
#include "pion/http/response_writer.hpp"
#include "pion/http/streaming_server.hpp"

jvmtiEnv* jvmti = nullptr;
jvmtiCapabilities caps;
jvmtiEventCallbacks callbacks;

pion::http::streaming_server* server = nullptr;

// not actually required as server is single-threaded
std::mutex web_mutex;

std::string shared_query;
std::mutex shared_query_mutex;
std::condition_variable shared_query_cond;
bool shared_query_flag;
std::string shared_resp;
std::mutex shared_resp_mutex;
std::condition_variable shared_resp_cond;
bool shared_resp_flag;

void set_shared_query(std::string st) {
    std::unique_lock<std::mutex> lock(shared_query_mutex, std::try_to_lock);
    shared_query = std::move(st);
    shared_query_flag = true;
    shared_query_cond.notify_one();
    std::cout << "1" << std::endl;
}

std::string get_shared_query() {
    std::unique_lock<std::mutex> lock(shared_query_mutex, std::try_to_lock);
    while(!shared_query_flag) {
        shared_query_cond.wait(lock);
    }
    shared_query_flag = false;
    std::cout << "2" << std::endl;
    // copy is intentional here
    return shared_query;
}

void set_shared_resp(std::string st) {
    std::unique_lock<std::mutex> lock(shared_resp_mutex, std::try_to_lock);
    shared_resp = std::move(st);
    shared_resp_flag = true;
    shared_resp_cond.notify_one();
    std::cout << "3" << std::endl;
}

std::string get_shared_resp() {
    std::unique_lock<std::mutex> lock(shared_resp_mutex, std::try_to_lock);
    while (!shared_resp_flag) {
        shared_resp_cond.wait(lock);
    }
    shared_resp_flag = false;
    std::cout << "4" << std::endl;
    // copy is intentional here
    return shared_resp;
}



/* Creates a new jthread */
static jthread
alloc_thread(JNIEnv* env) {
    // todo: result checks
    jclass thrClass = env->FindClass("java/lang/Thread");
    jmethodID cid = env->GetMethodID(thrClass, "<init>", "()V");
    jthread res = env->NewObject(thrClass, cid);
    return res;
}

void JNICALL worker_fun(jvmtiEnv* jvmti, JNIEnv* jni, void* p) {   
    (void) jni;
    (void) p;
    std::cout << "worker running" << std::endl;
    for(;;) {
        auto query = get_shared_query();
        char* buf = nullptr;
        auto error = jvmti->GetSystemProperty(query.c_str(), &buf);
        if (JVMTI_ERROR_NONE == error) {
            std::string st{buf};
            jvmti->Deallocate(reinterpret_cast<unsigned char*> (buf));
            set_shared_resp(st);
        } else {
            char* errbuf = nullptr;
            jvmti->GetErrorName(JVMTI_ERROR_NONE, &errbuf);
            std::string st{errbuf};
            set_shared_resp(st);
            jvmti->Deallocate(reinterpret_cast<unsigned char*> (errbuf));
        }
    }
}

/* Callback for JVMTI_EVENT_VM_INIT */
static void JNICALL
vm_init(jvmtiEnv *jvmti, JNIEnv *env, jthread thread) {
    (void) thread;
    // todo: err
    jvmti->RunAgentThread(alloc_thread(env), worker_fun, NULL, JVMTI_THREAD_MAX_PRIORITY);
}

void start_http_server() {
    server = new pion::http::streaming_server{1, 8080};
    server->add_method_specific_resource("GET", "/jvmti1",
            // lambda to handle GET requests
            [](pion::http::request_ptr& req, pion::tcp::connection_ptr & conn) {                
                // pion specific response writer creation
                auto finfun = std::bind(&pion::tcp::connection::finish, conn);
                auto writer = pion::http::response_writer::create(conn, *req, finfun);
                // logic
                std::unique_lock<std::mutex> lock(web_mutex, std::try_to_lock);
                set_shared_query("java.home");
                auto resp = get_shared_resp();
                writer << "---\n";
                writer << resp << "\n";
                writer << "---\n";
                // sending response
                writer->send();
            });
    server->start();
//    server.stop(true);
}

void add_capabilities(JavaVM *jvm) {
    jvm->GetEnv((void **) &jvmti, JVMTI_VERSION);    
    memset(&caps, 0, sizeof (caps));
    caps.can_generate_all_class_hook_events = 1;
    caps.can_tag_objects = 1;
    caps.can_get_source_file_name = 1;
    caps.can_get_line_numbers = 1;
    caps.can_generate_garbage_collection_events = 1;
    caps.can_tag_objects = 1;
    caps.can_generate_resource_exhaustion_heap_events = 1;   
    auto error = jvmti->AddCapabilities(&caps);
    if (JVMTI_ERROR_NONE != error) {
        char* errbuf = nullptr;
        jvmti->GetErrorName(error, &errbuf);
        std::cout << "Capabilities error: " << errbuf << std::endl;
        jvmti->Deallocate(reinterpret_cast<unsigned char*> (errbuf));
    }
    memset(&callbacks, 0, sizeof (callbacks));
    callbacks.VMInit = &vm_init;
    // todo: errs
    jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
    (void) options;
    (void) reserved;
    add_capabilities(jvm);
    std::cout << "I am loaded" << std::endl;
    start_http_server();    
    std::cout << "Agent HTTP server started" << std::endl;
    
//    char* errbuf = nullptr;
//    jvmti->GetErrorName(JVMTI_ERROR_NONE, &errbuf);
//    std::cout << "No error: " << errbuf << std::endl;
//    jvmti->Deallocate(reinterpret_cast<unsigned char*> (errbuf));
    
    
    return JNI_OK;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm) {
    (void) vm;
    server->stop(false);
    std::cout << "Agent HTTP server stopped" << std::endl;
    
//    char* errbuf = nullptr;
//    jvmti->GetErrorName(JVMTI_ERROR_NONE, &errbuf);
//    std::cout << "No error: " << errbuf << std::endl;
//    jvmti->Deallocate(reinterpret_cast<unsigned char*> (errbuf));
}
