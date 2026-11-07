#pragma once
#include "../sched_action.h"

namespace sched_action {
    class StartRecorderClass : public ActionClass {
    public:
        StartRecorderClass() {}
        ~StartRecorderClass() {}

        void trigger() {
        }

        void prepareEditMenu() {
        }

        bool showEditMenu(bool& valid) {
            valid = false;
            return false;
        }

        std::string getName() {
            return name;
        }

    private:
        std::string name = "Start \"Recorder\"";
    };

    Action StartRecorder() {
        return Action(new StartRecorderClass);
    }
}