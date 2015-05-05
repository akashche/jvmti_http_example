Proof of concept for HTTP access to JVMTI
=========================================

This project contains proof of concept implementation of HTTP server embedded into
JVMTI agent. It uses JVMTI API to access JVM system properties. It was tested on Fedora 21.

Build and run steps:

    git clone https://github.com/akashche/jvmti_http_example.git
    cd jvmti_http_example
    mkdir build
    cd build
    cmake ..
    make
    javac -d . ../test/App.java && java -agentpath:`pwd`/libjvmti1.so App

HTTP server will be started on port 8080 with path jvmti1:

    curl http://127.0.0.1:8080/jvmti1
    ---
    /usr/lib/jvm/java-1.8.0-openjdk-1.8.0.45-31.b13.fc21.x86_64/jre
    ---


