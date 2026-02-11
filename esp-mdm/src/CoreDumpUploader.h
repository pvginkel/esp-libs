#pragma once

#include "ApplicationBase.h"

class CoreDumpUploader {
    ApplicationBase* _application;
    const char* _url;

public:
    CoreDumpUploader(ApplicationBase* application, const char* url) : _application(application), _url(url) {}

    esp_err_t upload();
};
