#include <stdio.h>
#include <string.h>
#include <systemd/sd-bus.h>

static void publish_current_state(sd_bus *bus, char *last_state, size_t size) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    const char *state = NULL;

    int result = sd_bus_call_method(
        bus,
        "org.fcitx.Fcitx5",
        "/controller",
        "org.fcitx.Fcitx.Controller1",
        "CurrentInputMethod",
        &error,
        &reply,
        "");

    if (result >= 0)
        result = sd_bus_message_read(reply, "s", &state);

    if (result >= 0 && state && state[0] != '\0' &&
        strncmp(state, last_state, size) != 0) {
        snprintf(last_state, size, "%s", state);
        puts(last_state);
        fflush(stdout);
    }

    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
}

int main(void) {
    sd_bus *bus = NULL;
    char last_state[128] = "";

    if (sd_bus_default_user(&bus) < 0)
        return 1;

    for (;;) {
        publish_current_state(bus, last_state, sizeof(last_state));

        // CurrentInputMethod has no change signal in Fcitx 5.1.21. Waiting on
        // the bus keeps this process asleep between inexpensive method calls.
        sd_bus_wait(bus, 750000);
        while (sd_bus_process(bus, NULL) > 0) {}
    }
}
