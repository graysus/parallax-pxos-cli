
#include <cerrno>
#include <cmath>
#include <fstream>
#include <memory>
#include <stdlib.h>
#include <PxResult.hpp>
#include <curl/curl.h>
#include <curl/easy.h>
#include <string>
#include <thread>
#include <PxJob.hpp>
#include <PxLog.hpp>
#include <vector>

#ifndef PXDL
#define PXDL
namespace PxDownload {
    struct Stats {
        curl_off_t down;
        curl_off_t total;
        curl_off_t speed;
    };

	class LogDownloadTask : public PxLog::LogTask {
	public:
        std::string stats;
		LogDownloadTask(std::string name) {
			me = name;
			terse = "download "+name;
		}
		std::string repr() override {
			switch (status) {
				case PxLog::Success:
					return "Downloaded "+me+" ("+stats+")";
				case PxLog::Partial:
					return "Cancelled downloading "+me+" ("+stats+")";
				case PxLog::Fail:
					return "Failed to download "+me+" ("+stats+")";
				case PxLog::Pending:
					return "Downloading "+me+"... ("+stats+")";
			}
			return me;
		}
	};

    struct Subdownload {
        LogDownloadTask *tsk;
        int logid;
        bool done;
        PxResult::Result<void> result;
        CURL *curl;
        std::ofstream writeTo;
        Stats stats;
        std::thread thread;
        std::string source;
        inline Subdownload() {
            curl = curl_easy_init();
        }
        inline ~Subdownload() {
            curl_easy_cleanup(curl);
        }
        PxResult::Result<void> bindOutput(std::string dest) {
            writeTo = std::ofstream(dest, std::ios::out | std::ios::binary);
            
            return PxResult::Null;
        }
        size_t onwrite(char *data, size_t count) {
            curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &stats.down);
            curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &stats.total);
            curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD_T, &stats.speed);

            writeTo.write(data, count);
            return count;
        }

        void sthrd() {
            curl_easy_setopt(curl, CURLOPT_URL, source.c_str());

            curl_write_callback cfunc = [](char *data, size_t _, size_t count, void *_current) -> size_t {
                // return count;
                auto current = (Subdownload*)_current;
                return current->onwrite(data, count);
            };
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cfunc);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                result = PxResult::FResult("PxDownload::Download::perform / curl_easy_perform", EINVAL);
                done = true;
                return;
            }

            int response;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
            if (response != 200) {
                result = PxResult::FResult("PxDownload::Download::perform / curl_easy_perform", EINVAL);
                done = true;
                return;
            }
            done = true;
        }

        operator bool() {
            return curl != NULL;
        }

        void initTask() {
            logid = PxLog::log.newTask(tsk = new LogDownloadTask(source));
        }

        void updateTask() {
            if (tsk == NULL) return;

            std::vector<std::string> strstats = {};

            float spd = std::floor(stats.speed / 1024. / 1024. * 10.)/10.;
            strstats.push_back(std::to_string(spd)+" MiB/s");

            if (stats.total >= 0) {
                strstats.push_back(std::to_string((int)std::round((float)stats.down * 100. / (float)stats.total))+"%");
            }

            tsk->stats = PxFunction::join(strstats, ", ");

            if (done) {
                if (result.eno) {
                    PxLog::log.completeTask(logid, PxLog::Fail);
                } else {
                    PxLog::log.completeTask(logid, PxLog::Success);
                }
                tsk = NULL;
            }
        }
    };

    class Download {
    private:
        std::vector<std::shared_ptr<Subdownload>> downloads;
    public:
        Download() {}

        std::shared_ptr<Subdownload> add(std::string source) {
            auto dld = std::make_shared<Subdownload>();
            dld->source = source;
            downloads.push_back(dld);
            return dld;
        }

        PxResult::Result<void> perform() {
            for (auto &i : downloads) {
                i->done = false;
                i->result = PxResult::Null;

                i->thread = std::thread([&i]() {
                    i->sthrd();
                });
                i->initTask();
            }

            PxJob::JobServer js;
            js.AddJob(std::make_shared<PxJob::OscJob>(&PxLog::log));

            bool done = false;

            while (!done) {
                js.tick();

                for (auto i : downloads) {
                    i->updateTask();
                }
                PxLog::log.top();
                PxLog::log.printTasks();

                done = true;
                for (auto i : downloads) {
                    done = done && i->done;
                }

                usleep(50000);
            }

            for (auto i : downloads) {
                i->thread.join();
                i->updateTask();
            }
            for (auto i : downloads) {
                PXASSERT(i->result);
            }

            return PxResult::Null;
        }
    };
}
#endif