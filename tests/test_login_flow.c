/* Headless check of the login flow: no window, no hardware, just state transitions. */
#include "terminal.h"
#include <stdio.h>

static void click(App *app, int x, int y)
{
    Event event;
    event.type = EVENT_DOWN; event.x = x; event.y = y;
    app_event(app, event);
    event.type = EVENT_UP;
    app_event(app, event);
}

static void type_123456(App *app)
{
    click(app, 88, 257);   /* 1 */
    click(app, 190, 257);  /* 2 */
    click(app, 292, 257);  /* 3 */
    click(app, 88, 317);   /* 4 */
    click(app, 190, 317);  /* 5 */
    click(app, 292, 317);  /* 6 */
}

int main(void)
{
    static App app;
    if (!app_open(&app, "data/drawings")) { printf("OPEN FAIL\n"); return 1; }
    printf("1) page=%d (expect %d LOGIN)\n", (int)app.page, (int)PAGE_LOGIN);

    type_123456(&app);
    printf("2) user=[%s] (expect 123456)\n", app.login.user);

    click(&app, 190, 188);  /* password field */
    printf("3) field=%d (expect 1)\n", app.login.field);

    type_123456(&app);
    printf("4) pass=[%s] (expect 123456)\n", app.login.pass);

    click(&app, 292, 437);  /* confirm key */
    printf("5) fxFrame=%d (expect 1) msg=[%s]\n", app.login.fxFrame, app.login.message);

    app_render(&app);
    for (int i = 0; i < 70; ++i) { app_tick(&app); app_render(&app); }
    printf("6) page=%d (expect %d HOME)\n", (int)app.page, (int)PAGE_HOME);

    /* wrong password must stay on login */
    app.page = PAGE_LOGIN;
    app.login.fxFrame = 0;
    app.login.field = 1;
    app.login.pass[0] = '\0';
    app.login.pass[1] = '\0';
    app.login.pass[2] = '\0';
    app.login.pass[3] = '\0';
    app.login.pass[4] = '\0';
    app.login.pass[5] = '\0';
    type_123456(&app);
    app.login.pass[5] = '9';
    click(&app, 292, 437);
    printf("7) wrong-pass page=%d (expect %d LOGIN) msg=[%s]\n",
           (int)app.page, (int)PAGE_LOGIN, app.login.message);

    app_close(&app);
    return 0;
}
