#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

static GtkWidget *main_win, *main_stack, *clock_lbl, *sys_lbl;
static GtkCssProvider *css_provider;
static gchar *current_theme = NULL;

static void apply_theme(const char *name) {
    g_free(current_theme);
    current_theme = g_strdup(name);
    mkdir("/etc/mutantde", 0755);
    FILE *f = fopen("/etc/mutantde/theme", "w");
    if (f) { fprintf(f, "%s\n", name); fclose(f); }

    const char *bg = "#1e293b", *panel = "#0f172a", *card = "#162032";
    const char *fg = "#f8fafc", *bd = "#334155", *btn = "#f8fafc", *bfg = "#0f172a";
    if (!strcmp(name, "white")) {
        bg="#f8fafc"; panel="#ffffff"; card="#f1f5f9"; fg="#0f172a"; bd="#cbd5e1"; btn="#0f172a"; bfg="#ffffff";
    } else if (!strcmp(name, "gray")) {
        bg="#64748b"; panel="#475569"; card="#526075"; fg="#ffffff"; bd="#94a3b8"; btn="#ffffff"; bfg="#1e293b";
    } else if (!strcmp(name, "black")) {
        bg="#000000"; panel="#09090b"; card="#121215"; fg="#ffffff"; bd="#27272a"; btn="#ffffff"; bfg="#000000";
    }

    gchar *css = g_strdup_printf(
        "window,.desktop-bg{background:%s;color:%s;font-family:Sans;}"
        ".top-panel{background:%s;border-bottom:1px solid %s;padding:3px 6px;}"
        ".dock-bar{background:%s;border:1px solid %s;border-radius:999px;padding:6px 16px;box-shadow:0 6px 18px rgba(0,0,0,0.35);}"
        ".dock-btn{background:%s;color:%s;border:1px solid %s;border-radius:999px;padding:6px 14px;font-weight:bold;}"
        ".dock-btn:hover{background:%s;color:%s;}"
        ".app-tile,.sys-card{background:%s;border:1px solid %s;border-radius:12px;padding:10px;}"
        ".app-tile:hover{border-color:%s;}"
        ".accent-btn{background:%s;color:%s;font-weight:bold;border-radius:8px;padding:6px 14px;border:none;}"
        ".flat-btn{background:%s;color:%s;border:1px solid %s;border-radius:8px;padding:5px 10px;}"
        "textview text{background:#09090b;color:#f8fafc;font-family:monospace;font-size:10pt;}"
        "entry{background:%s;color:%s;border:1px solid %s;border-radius:8px;padding:6px 10px;}",
        bg, fg, panel, bd, panel, bd, card, fg, bd, btn, bfg, card, bd, btn, btn, bfg, card, fg, bd, card, fg, bd);
    gtk_css_provider_load_from_data(css_provider, css, -1, NULL);
    g_free(css);
}

static void run_cmd_buf(const char *cmd, GtkTextBuffer *buf) {
    gchar *out = NULL, *err = NULL;
    g_spawn_command_line_sync(cmd, &out, &err, NULL, NULL);
    GtkTextIter end;
    if (out && *out) { gtk_text_buffer_get_end_iter(buf, &end); gtk_text_buffer_insert(buf, &end, out, -1); }
    if (err && *err) { gtk_text_buffer_get_end_iter(buf, &end); gtk_text_buffer_insert(buf, &end, err, -1); }
    g_free(out); g_free(err);
}

static void run_sh_buf(const char *sh_cmd, GtkTextBuffer *buf) {
    gchar *q = g_shell_quote(sh_cmd);
    gchar *full = g_strdup_printf("/bin/sh -c %s", q);
    run_cmd_buf(full, buf);
    g_free(q); g_free(full);
}

static void open_view(const char *title, const char *sh_cmd);

static void open_browser(GtkWidget *w, gpointer d) {
    if (g_file_test("/usr/bin/firefox-esr", G_FILE_TEST_IS_EXECUTABLE)) {
        system("mkdir -p /tmp/runtime-root && chmod 700 /tmp/runtime-root && "
               "XDG_RUNTIME_DIR=/tmp/runtime-root MOZ_ENABLE_WAYLAND=0 MOZ_WEBRENDER=0 "
               "LIBGL_ALWAYS_SOFTWARE=1 DISPLAY=:0 dbus-run-session /usr/bin/firefox-esr --no-remote "
               "https://github.io >/var/log/firefox.log 2>&1 &");
    } else {
        open_view("MutantBrowser (Text Fallback)", "SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt links -dump -width 88 https://github.io 2>&1");
    }
}

typedef struct {
    GtkTextBuffer *buf;
    GtkWidget *prompt_lbl;
    GtkWidget *entry;
    gchar *cwd;
    GPtrArray *hist;
    int hist_pos;
} TermCtx;

static void term_exec_str(TermCtx *ctx, const char *cmd) {
    if (!cmd || !*cmd) return;
    g_ptr_array_add(ctx->hist, g_strdup(cmd));
    ctx->hist_pos = ctx->hist->len;

    gchar *hdr = g_strdup_printf("root@mutant:%s# %s\n", ctx->cwd, cmd);
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(ctx->buf, &end);
    gtk_text_buffer_insert(ctx->buf, &end, hdr, -1);
    g_free(hdr);

    if (!strcmp(cmd, "clear")) {
        gtk_text_buffer_set_text(ctx->buf, "", -1);
    } else if (!strcmp(cmd, "help")) {
        run_sh_buf("/usr/bin/mutant-help | sed 's/\\x1b\\[[0-9;]*m//g'", ctx->buf);
    } else if (!strcmp(cmd, "cd") || g_str_has_prefix(cmd, "cd ")) {
        const char *arg = (!strcmp(cmd, "cd")) ? "/root" : g_strstrip((gchar *)cmd + 3);
        gchar *target = (*arg == '/') ? g_strdup(arg) : g_build_filename(ctx->cwd, arg, NULL);
        if (g_file_test(target, G_FILE_TEST_IS_DIR)) {
            g_free(ctx->cwd);
            ctx->cwd = g_canonicalize_filename(target, NULL);
            gchar *pl = g_strdup_printf("%s #", ctx->cwd);
            gtk_label_set_text(GTK_LABEL(ctx->prompt_lbl), pl);
            g_free(pl);
        } else {
            run_sh_buf("echo 'cd: directory not found'", ctx->buf);
        }
        g_free(target);
    } else {
        gchar *qcwd = g_shell_quote(ctx->cwd);
        gchar *full = g_strdup_printf("cd %s && %s 2>&1", qcwd, cmd);
        run_sh_buf(full, ctx->buf);
        g_free(qcwd); g_free(full);
    }
}

static void on_term_enter(GtkEntry *e, gpointer data) {
    TermCtx *ctx = (TermCtx *)data;
    const char *cmd = gtk_entry_get_text(e);
    term_exec_str(ctx, cmd);
    gtk_entry_set_text(e, "");
}

static gboolean on_term_key(GtkWidget *w, GdkEventKey *ev, gpointer data) {
    TermCtx *ctx = (TermCtx *)data;
    if (ev->keyval == GDK_KEY_Up && ctx->hist->len > 0) {
        if (ctx->hist_pos > 0) ctx->hist_pos--;
        gtk_entry_set_text(GTK_ENTRY(ctx->entry), g_ptr_array_index(ctx->hist, ctx->hist_pos));
        gtk_editable_set_position(GTK_EDITABLE(ctx->entry), -1);
        return TRUE;
    } else if (ev->keyval == GDK_KEY_Down && ctx->hist->len > 0) {
        if (ctx->hist_pos < (int)ctx->hist->len - 1) {
            ctx->hist_pos++;
            gtk_entry_set_text(GTK_ENTRY(ctx->entry), g_ptr_array_index(ctx->hist, ctx->hist_pos));
        } else {
            ctx->hist_pos = ctx->hist->len;
            gtk_entry_set_text(GTK_ENTRY(ctx->entry), "");
        }
        gtk_editable_set_position(GTK_EDITABLE(ctx->entry), -1);
        return TRUE;
    }
    return FALSE;
}

static void term_quick(GtkWidget *w, gpointer data) {
    TermCtx *ctx = (TermCtx *)g_object_get_data(G_OBJECT(w), "ctx");
    term_exec_str(ctx, (const char *)data);
}

static void open_terminal(GtkWidget *w, gpointer d) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "MutantTerminal");
    gtk_window_set_default_size(GTK_WINDOW(win), 740, 440);
    GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vb), 8);
    gtk_container_add(GTK_CONTAINER(win), vb);

    GtkWidget *tbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_container_add(GTK_CONTAINER(sc), tv);

    GtkWidget *in_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *plbl = gtk_label_new("/root #");
    GtkWidget *ent = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ent), "Enter command (try: help, ls, mpkg list, mutantctl status)...");
    gtk_box_pack_start(GTK_BOX(in_box), plbl, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(in_box), ent, TRUE, TRUE, 0);

    TermCtx *ctx = g_new0(TermCtx, 1);
    ctx->buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    ctx->prompt_lbl = plbl; ctx->entry = ent;
    ctx->cwd = g_strdup("/root");
    ctx->hist = g_ptr_array_new_with_free_func(g_free);

    const char *qcmds[] = {"help", "mutantctl status", "mpkg list", "free -m", "clear"};
    for (int i = 0; i < 5; i++) {
        GtkWidget *qb = gtk_button_new_with_label(qcmds[i]);
        gtk_style_context_add_class(gtk_widget_get_style_context(qb), "flat-btn");
        g_object_set_data(G_OBJECT(qb), "ctx", ctx);
        g_signal_connect(qb, "clicked", G_CALLBACK(term_quick), (gpointer)qcmds[i]);
        gtk_box_pack_start(GTK_BOX(tbar), qb, FALSE, FALSE, 0);
    }

    gtk_box_pack_start(GTK_BOX(vb), tbar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vb), sc, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vb), in_box, FALSE, FALSE, 0);

    g_signal_connect(ent, "activate", G_CALLBACK(on_term_enter), ctx);
    g_signal_connect(ent, "key-press-event", G_CALLBACK(on_term_key), ctx);
    term_exec_str(ctx, "uname -a && free -m");
    gtk_widget_show_all(win);
    gtk_widget_grab_focus(ent);
}

typedef struct { GtkWidget *path_ent; GtkTextBuffer *buf; } PadCtx;
typedef struct { GtkWidget *path_ent; GtkListStore *store; } DirCtx;

static void dir_populate(DirCtx *ctx, const char *dir_path) {
    gtk_list_store_clear(ctx->store);
    GDir *d = g_dir_open(dir_path, 0, NULL);
    if (!d) return;
    const gchar *name;
    while ((name = g_dir_read_name(d)) != NULL) {
        gchar *full = g_build_filename(dir_path, name, NULL);
        gboolean is_dir = g_file_test(full, G_FILE_TEST_IS_DIR);
        GtkTreeIter iter;
        gtk_list_store_append(ctx->store, &iter);
        gtk_list_store_set(ctx->store, &iter, 0, is_dir ? "[DIR]" : "[FILE]", 1, name, 2, full, 3, is_dir, -1);
        g_free(full);
    }
    g_dir_close(d);
}

static void on_dir_go(GtkWidget *w, gpointer data) {
    DirCtx *ctx = (DirCtx *)data;
    const char *p = gtk_entry_get_text(GTK_ENTRY(ctx->path_ent));
    dir_populate(ctx, (p && *p) ? p : "/root");
}

static void on_dir_up(GtkWidget *w, gpointer data) {
    DirCtx *ctx = (DirCtx *)data;
    const char *p = gtk_entry_get_text(GTK_ENTRY(ctx->path_ent));
    gchar *parent = g_path_get_dirname((p && *p) ? p : "/root");
    gtk_entry_set_text(GTK_ENTRY(ctx->path_ent), parent);
    dir_populate(ctx, parent);
    g_free(parent);
}

static void on_dir_jump(GtkWidget *w, gpointer data) {
    DirCtx *ctx = (DirCtx *)g_object_get_data(G_OBJECT(w), "ctx");
    gtk_entry_set_text(GTK_ENTRY(ctx->path_ent), (const char *)data);
    dir_populate(ctx, (const char *)data);
}

static void open_editor_file(const char *filepath);

static void on_file_activated(GtkTreeView *tv, GtkTreePath *tp, GtkTreeViewColumn *col, gpointer data) {
    DirCtx *ctx = (DirCtx *)data;
    GtkTreeModel *m = gtk_tree_view_get_model(tv);
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter(m, &iter, tp)) return;
    gchar *full = NULL;
    gboolean is_dir = FALSE;
    gtk_tree_model_get(m, &iter, 2, &full, 3, &is_dir, -1);
    if (is_dir) {
        gtk_entry_set_text(GTK_ENTRY(ctx->path_ent), full);
        dir_populate(ctx, full);
    } else if (g_str_has_suffix(full, ".mbin") || g_str_has_suffix(full, ".deb") || g_str_has_suffix(full, ".exe")) {
        gchar *q = g_shell_quote(full);
        gchar *cmd = g_strdup_printf("/usr/sbin/mutant-exec %s 2>&1", q);
        gchar *title = g_strdup_printf("MutantExec — %s", g_path_get_basename(full));
        open_view(title, cmd);
        g_free(q); g_free(cmd); g_free(title);
    } else {
        open_editor_file(full);
    }
    g_free(full);
}

static void open_files(GtkWidget *w, gpointer d) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "MutantFiles (Click to Open)");
    gtk_window_set_default_size(GTK_WINDOW(win), 760, 450);
    GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vb), 8);
    gtk_container_add(GTK_CONTAINER(win), vb);

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *up_b = gtk_button_new_with_label("Up");
    gtk_style_context_add_class(gtk_widget_get_style_context(up_b), "flat-btn");
    GtkWidget *ent = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(ent), "/root");
    GtkWidget *go = gtk_button_new_with_label("Go");
    gtk_style_context_add_class(gtk_widget_get_style_context(go), "accent-btn");
    gtk_box_pack_start(GTK_BOX(bar), up_b, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), ent, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(bar), go, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vb), bar, FALSE, FALSE, 0);

    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    GtkListStore *store = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    GtkWidget *tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(tv), TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tv), gtk_tree_view_column_new_with_attributes("Type", gtk_cell_renderer_text_new(), "text", 0, NULL));
    gtk_tree_view_append_column(GTK_TREE_VIEW(tv), gtk_tree_view_column_new_with_attributes("Name", gtk_cell_renderer_text_new(), "text", 1, NULL));

    GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(sc), tv);

    DirCtx *ctx = g_new0(DirCtx, 1);
    ctx->path_ent = ent; ctx->store = store;

    const char *places[] = {"/root", "/", "/etc", "/usr/share/applications", "/var/log"};
    for (int i = 0; i < 5; i++) {
        GtkWidget *pb = gtk_button_new_with_label(places[i]);
        gtk_style_context_add_class(gtk_widget_get_style_context(pb), "flat-btn");
        g_object_set_data(G_OBJECT(pb), "ctx", ctx);
        g_signal_connect(pb, "clicked", G_CALLBACK(on_dir_jump), (gpointer)places[i]);
        gtk_box_pack_start(GTK_BOX(side), pb, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(hb), side, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hb), sc, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vb), hb, TRUE, TRUE, 0);

    g_signal_connect(up_b, "clicked", G_CALLBACK(on_dir_up), ctx);
    g_signal_connect(go, "clicked", G_CALLBACK(on_dir_go), ctx);
    g_signal_connect(ent, "activate", G_CALLBACK(on_dir_go), ctx);
    g_signal_connect(tv, "row-activated", G_CALLBACK(on_file_activated), ctx);
    dir_populate(ctx, "/root");
    gtk_widget_show_all(win);
}

static void pad_open(GtkWidget *w, gpointer data) {
    PadCtx *ctx = (PadCtx *)data;
    const char *p = gtk_entry_get_text(GTK_ENTRY(ctx->path_ent));
    gchar *contents = NULL;
    if (g_file_get_contents(p, &contents, NULL, NULL)) {
        gtk_text_buffer_set_text(ctx->buf, contents, -1);
        g_free(contents);
    } else {
        gtk_text_buffer_set_text(ctx->buf, "File not found.\n", -1);
    }
}

static void pad_save(GtkWidget *w, gpointer data) {
    PadCtx *ctx = (PadCtx *)data;
    const char *p = gtk_entry_get_text(GTK_ENTRY(ctx->path_ent));
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(ctx->buf, &s, &e);
    gchar *txt = gtk_text_buffer_get_text(ctx->buf, &s, &e, FALSE);
    g_file_set_contents(p, txt, -1, NULL);
    g_free(txt);
}

static void open_editor_file(const char *filepath) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "MutantPad Editor");
    gtk_window_set_default_size(GTK_WINDOW(win), 700, 420);
    GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vb), 8);
    gtk_container_add(GTK_CONTAINER(win), vb);

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *ent = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(ent), filepath ? filepath : "/etc/hostname");
    GtkWidget *ob = gtk_button_new_with_label("Load");
    GtkWidget *sb = gtk_button_new_with_label("Save");
    gtk_style_context_add_class(gtk_widget_get_style_context(ob), "flat-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(sb), "accent-btn");
    gtk_box_pack_start(GTK_BOX(bar), ent, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(bar), ob, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), sb, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vb), bar, FALSE, FALSE, 0);

    GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *tv = gtk_text_view_new();
    gtk_container_add(GTK_CONTAINER(sc), tv);
    gtk_box_pack_start(GTK_BOX(vb), sc, TRUE, TRUE, 0);

    PadCtx *ctx = g_new0(PadCtx, 1);
    ctx->path_ent = ent; ctx->buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    g_signal_connect(ob, "clicked", G_CALLBACK(pad_open), ctx);
    g_signal_connect(sb, "clicked", G_CALLBACK(pad_save), ctx);
    pad_open(NULL, ctx);
    gtk_widget_show_all(win);
}
static void open_editor(GtkWidget *w, gpointer d) { open_editor_file("/etc/hostname"); }

static void store_exec(GtkWidget *w, gpointer data) {
    GtkTextBuffer *buf = GTK_TEXT_BUFFER(g_object_get_data(G_OBJECT(w), "buf"));
    gtk_text_buffer_set_text(buf, "Running package task...\n", -1);
    while (gtk_events_pending()) gtk_main_iteration();
    run_sh_buf((const char *)data, buf);
}

static void open_store(GtkWidget *w, gpointer d) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "MutantLinux Package Store");
    gtk_window_set_default_size(GTK_WINDOW(win), 660, 420);
    GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vb), 10);
    gtk_container_add(GTK_CONTAINER(win), vb);

    GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_container_add(GTK_CONTAINER(sc), tv);
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));

    const char *lbls[] = {"Sync Catalog (mpkg update)", "Install nano (v8.2.0)", "Install htop (v3.3.0)", "List Packages"};
    const char *cmds[] = {"mpkg update 2>&1", "mpkg install nano 2>&1", "mpkg install htop 2>&1", "mpkg list 2>&1"};
    for (int i = 0; i < 4; i++) {
        GtkWidget *b = gtk_button_new_with_label(lbls[i]);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), i == 0 ? "accent-btn" : "flat-btn");
        g_object_set_data(G_OBJECT(b), "buf", buf);
        g_signal_connect(b, "clicked", G_CALLBACK(store_exec), (gpointer)cmds[i]);
        gtk_box_pack_start(GTK_BOX(vb), b, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(vb), sc, TRUE, TRUE, 0);
    run_sh_buf("mpkg list 2>&1", buf);
    gtk_widget_show_all(win);
}

static void open_view(const char *title, const char *sh_cmd) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), title);
    gtk_window_set_default_size(GTK_WINDOW(win), 660, 400);
    GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_container_add(GTK_CONTAINER(sc), tv);
    gtk_container_add(GTK_CONTAINER(win), sc);
    run_sh_buf(sh_cmd, gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv)));
    gtk_widget_show_all(win);
}

static void open_net(GtkWidget *w, gpointer d) { open_view("MutantNetwork", "ip -4 addr && echo '--- DNS ---' && cat /etc/resolv.conf"); }
static void open_tasks(GtkWidget *w, gpointer d) { open_view("Task Manager", "free -m && echo '--- PROCESSES ---' && ps -o pid,user,comm"); }
static void open_ai(GtkWidget *w, gpointer d) { open_view("AI Auto-Fixer Diagnostics", "cat /var/log/mutantd/ai-fixer.log 2>&1"); }
static void open_sysinfo(GtkWidget *w, gpointer d) {
    open_view("System Information — MutantLinux 1.0",
        "echo 'MutantLinux 1.0 (Independent ARM64 Operating System)' && uname -a && echo '' && free -m && echo '' && df -h / && echo '' && echo '=== DETECTED HARDWARE PROFILE ===' && cat /run/mutantd/hardware.profile 2>/dev/null");
}

static void on_thm(GtkWidget *w, gpointer d) { apply_theme((const char *)d); }
static void on_res(GtkWidget *w, gpointer d) {
    gchar *cmd = g_strdup_printf("xrandr -s %s >/dev/null 2>&1", (const char *)d);
    system(cmd); g_free(cmd);
}
static void on_dns(GtkWidget *w, gpointer d) {
    gchar *cmd = g_strdup_printf("printf 'nameserver %s\\nnameserver 1.0.0.1\\n' > /etc/resolv.conf", (const char *)d);
    system(cmd); g_free(cmd);
}
static void on_mode(GtkWidget *w, gpointer d) {
    gchar *cmd = g_strdup_printf("echo '%s' > /etc/mutant-mode", (const char *)d);
    system(cmd); g_free(cmd);
}
static void on_save_host(GtkWidget *w, gpointer d) {
    const char *h = gtk_entry_get_text(GTK_ENTRY(d));
    if (h && *h) {
        gchar *qh = g_shell_quote(h);
        gchar *cmd = g_strdup_printf("echo %s > /etc/hostname && hostname %s", qh, qh);
        system(cmd); g_free(qh); g_free(cmd);
    }
}

typedef struct { GtkWidget *u_ent; GtkWidget *p_ent; GtkWidget *st_lbl; } UserCtx;

static void on_add_admin(GtkWidget *w, gpointer data) {
    UserCtx *ctx = (UserCtx *)data;
    const char *u = gtk_entry_get_text(GTK_ENTRY(ctx->u_ent));
    const char *p = gtk_entry_get_text(GTK_ENTRY(ctx->p_ent));
    if (!u || !*u || !p || !*p) return;
    gchar *qu = g_shell_quote(u), *qp = g_shell_quote(p);
    gchar *cmd = g_strdup_printf(
        "mkdir -p /home/%s && adduser -D -h /home/%s -s /bin/sh %s 2>/dev/null; "
        "echo %s:%s | chpasswd && grep -q '^wheel:' /etc/group || echo 'wheel:x:10:' >> /etc/group; "
        "addgroup %s wheel 2>/dev/null; chown -R %s:%s /home/%s",
        qu, qu, qu, qu, qp, qu, qu, qu, qu);
    system(cmd);
    g_free(qu); g_free(qp); g_free(cmd);
    gtk_entry_set_text(GTK_ENTRY(ctx->u_ent), "");
    gtk_entry_set_text(GTK_ENTRY(ctx->p_ent), "");
    gtk_label_set_text(GTK_LABEL(ctx->st_lbl), "Account created with admin (wheel) permissions.");
}

static void open_settings(GtkWidget *w, gpointer d) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "MutantDE Settings Manager");
    gtk_window_set_default_size(GTK_WINDOW(win), 580, 460);
    GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(win), sc);

    GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vb), 16);
    gtk_container_add(GTK_CONTAINER(sc), vb);

    gtk_box_pack_start(GTK_BOX(vb), gtk_label_new("1. Appearance & Monochrome Theme"), FALSE, FALSE, 0);
    GtkWidget *hb1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    const char *th[] = {"white", "gray", "darkgray", "black"};
    for (int i = 0; i < 4; i++) {
        GtkWidget *b = gtk_button_new_with_label(th[i]);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "flat-btn");
        g_signal_connect(b, "clicked", G_CALLBACK(on_thm), (gpointer)th[i]);
        gtk_box_pack_start(GTK_BOX(hb1), b, TRUE, TRUE, 0);
    }
    gtk_box_pack_start(GTK_BOX(vb), hb1, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vb), gtk_label_new("2. Display Resolution"), FALSE, FALSE, 0);
    GtkWidget *hb2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    const char *rs[] = {"800x600", "1024x768", "1280x720", "1920x1080"};
    for (int i = 0; i < 4; i++) {
        GtkWidget *b = gtk_button_new_with_label(rs[i]);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "flat-btn");
        g_signal_connect(b, "clicked", G_CALLBACK(on_res), (gpointer)rs[i]);
        gtk_box_pack_start(GTK_BOX(hb2), b, TRUE, TRUE, 0);
    }
    gtk_box_pack_start(GTK_BOX(vb), hb2, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vb), gtk_label_new("3. DNS Resolver & Default Boot Mode"), FALSE, FALSE, 0);
    GtkWidget *hb3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    const char *dn[] = {"1.1.1.1", "8.8.8.8", "9.9.9.9"};
    for (int i = 0; i < 3; i++) {
        GtkWidget *b = gtk_button_new_with_label(dn[i]);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "flat-btn");
        g_signal_connect(b, "clicked", G_CALLBACK(on_dns), (gpointer)dn[i]);
        gtk_box_pack_start(GTK_BOX(hb3), b, TRUE, TRUE, 0);
    }
    GtkWidget *bgui = gtk_button_new_with_label("Boot: GUI");
    GtkWidget *bcli = gtk_button_new_with_label("Boot: CLI");
    gtk_style_context_add_class(gtk_widget_get_style_context(bgui), "flat-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(bcli), "flat-btn");
    g_signal_connect(bgui, "clicked", G_CALLBACK(on_mode), (gpointer)"gui");
    g_signal_connect(bcli, "clicked", G_CALLBACK(on_mode), (gpointer)"cli");
    gtk_box_pack_start(GTK_BOX(hb3), bgui, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hb3), bcli, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vb), hb3, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vb), gtk_label_new("4. System Hostname"), FALSE, FALSE, 0);
    GtkWidget *hb4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *hent = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(hent), "mutant-arm64");
    GtkWidget *hbtn = gtk_button_new_with_label("Apply Hostname");
    gtk_style_context_add_class(gtk_widget_get_style_context(hbtn), "accent-btn");
    g_signal_connect(hbtn, "clicked", G_CALLBACK(on_save_host), hent);
    gtk_box_pack_start(GTK_BOX(hb4), hent, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hb4), hbtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vb), hb4, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vb), gtk_label_new("5. Create Administrator Account"), FALSE, FALSE, 0);
    GtkWidget *hb5 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *uent = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(uent), "Username");
    GtkWidget *pent = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(pent), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(pent), FALSE);
    GtkWidget *ubtn = gtk_button_new_with_label("Create Admin");
    gtk_style_context_add_class(gtk_widget_get_style_context(ubtn), "accent-btn");
    GtkWidget *ust = gtk_label_new("");
    UserCtx *uctx = g_new0(UserCtx, 1); uctx->u_ent = uent; uctx->p_ent = pent; uctx->st_lbl = ust;
    g_signal_connect(ubtn, "clicked", G_CALLBACK(on_add_admin), uctx);
    gtk_box_pack_start(GTK_BOX(hb5), uent, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hb5), pent, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hb5), ubtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vb), hb5, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vb), ust, FALSE, FALSE, 0);

    gtk_widget_show_all(win);
}

static gboolean update_stats(gpointer data) {
    time_t now = time(NULL);
    gchar *tbuf = g_date_time_format(g_date_time_new_from_unix_local(now), "%a %H:%M:%S");
    if (clock_lbl && tbuf) gtk_label_set_text(GTK_LABEL(clock_lbl), tbuf);
    g_free(tbuf);

    if (sys_lbl) {
        gchar *host = NULL, *mem = NULL, *ip = NULL;
        g_spawn_command_line_sync("hostname", &host, NULL, NULL, NULL);
        g_spawn_command_line_sync("/bin/sh -c \"free -m | awk '/Mem:/ {print \\$3 \\\" / \\\" \\$2 \\\" MB\\\"}'\"", &mem, NULL, NULL, NULL);
        g_spawn_command_line_sync("/bin/sh -c \"ip -4 -o addr show | awk '\\$2 != \\\"lo\\\" {print \\$4; exit}'\"", &ip, NULL, NULL, NULL);
        if (host) g_strstrip(host);
        if (mem) g_strstrip(mem);
        if (ip) g_strstrip(ip);
        const char *ai = !system("pidof mutant-ai-fixer >/dev/null 2>&1") ? "ACTIVE" : "STANDBY";
        gchar *sbuf = g_strdup_printf("MUTANTLINUX 1.0 • ARM64\nHost : %s\nRAM  : %s\nNet  : %s\nAI   : %s",
            (host && *host) ? host : "mutant-arm64",
            (mem && *mem) ? mem : "N/A",
            (ip && *ip) ? ip : "Offline", ai);
        gtk_label_set_text(GTK_LABEL(sys_lbl), sbuf);
        g_free(sbuf); g_free(host); g_free(mem); g_free(ip);
    }
    return TRUE;
}

static gboolean end_load(gpointer d) {
    gtk_stack_set_visible_child_name(GTK_STACK(main_stack), "desktop");
    system("(for c in Master PCM Speaker Headphone; do amixer sset $c 90% unmute >/dev/null 2>&1; done; ogg123 -d alsa -q /usr/share/sounds/mutantde/welcome.ogg) >/var/log/chime.log 2>&1 &");
    return FALSE;
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    gchar *saved = NULL;
    if (g_file_get_contents("/etc/mutantde/theme", &saved, NULL, NULL)) g_strstrip(saved);

    css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    apply_theme((saved && *saved) ? saved : "darkgray");
    g_free(saved);

    main_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_fullscreen(GTK_WINDOW(main_win));
    g_signal_connect(main_win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    main_stack = gtk_stack_new();
    gtk_container_add(GTK_CONTAINER(main_win), main_stack);

    GtkWidget *lb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_valign(lb, GTK_ALIGN_CENTER); gtk_widget_set_halign(lb, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(lb), gtk_label_new("MUTANTLINUX 1.0  •  MUTANTDE"), FALSE, FALSE, 0);
    GtkWidget *pb = gtk_progress_bar_new(); gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pb), 0.9);
    gtk_box_pack_start(GTK_BOX(lb), pb, FALSE, FALSE, 0);
    gtk_stack_add_named(GTK_STACK(main_stack), lb, "loader");

    GtkWidget *dv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(dv), "desktop-bg");

    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(top), "top-panel");
    GtkWidget *br = gtk_button_new_with_label("MutantDE 1.0");
    gtk_style_context_add_class(gtk_widget_get_style_context(br), "accent-btn");
    g_signal_connect(br, "clicked", G_CALLBACK(open_settings), NULL);
    gtk_box_pack_start(GTK_BOX(top), br, FALSE, FALSE, 4);
    clock_lbl = gtk_label_new("");
    gtk_box_pack_end(GTK_BOX(top), clock_lbl, FALSE, FALSE, 8);
    const char *tn[] = {"black", "darkgray", "gray", "white"};
    for (int i = 0; i < 4; i++) {
        GtkWidget *tb = gtk_button_new_with_label(tn[i]);
        gtk_style_context_add_class(gtk_widget_get_style_context(tb), "flat-btn");
        g_signal_connect(tb, "clicked", G_CALLBACK(on_thm), (gpointer)tn[i]);
        gtk_box_pack_end(GTK_BOX(top), tb, FALSE, FALSE, 2);
    }
    gtk_box_pack_start(GTK_BOX(dv), top, FALSE, FALSE, 0);

    GtkWidget *mid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(mid), 16);
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10); gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    struct { const char *l; GCallback cb; } apps[] = {
        {">_ Terminal", G_CALLBACK(open_terminal)}, {"WW Browser", G_CALLBACK(open_browser)},
        {"[] Files", G_CALLBACK(open_files)}, {"Tx Editor", G_CALLBACK(open_editor)},
        {"NT Network", G_CALLBACK(open_net)}, {"++ Store", G_CALLBACK(open_store)},
        {"TM Tasks", G_CALLBACK(open_tasks)}, {"AI Fixer", G_CALLBACK(open_ai)},
        {"IF SysInfo", G_CALLBACK(open_sysinfo)}, {"ST Settings", G_CALLBACK(open_settings)}
    };
    for (int i = 0; i < 10; i++) {
        GtkWidget *b = gtk_button_new_with_label(apps[i].l);
        gtk_widget_set_size_request(b, 120, 64);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "app-tile");
        g_signal_connect(b, "clicked", apps[i].cb, NULL);
        gtk_grid_attach(GTK_GRID(grid), b, i / 5, i % 5, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(mid), grid, FALSE, FALSE, 0);
    sys_lbl = gtk_label_new("Loading...");
    gtk_style_context_add_class(gtk_widget_get_style_context(sys_lbl), "sys-card");
    gtk_widget_set_valign(sys_lbl, GTK_ALIGN_START);
    gtk_box_pack_end(GTK_BOX(mid), sys_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dv), mid, TRUE, TRUE, 0);

    /* Round Pill Floating Dock */
    GtkWidget *dock = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(dock), "dock-bar");
    gtk_widget_set_halign(dock, GTK_ALIGN_CENTER);
    for (int i = 0; i < 6; i++) {
        GtkWidget *db = gtk_button_new_with_label(apps[i].l);
        gtk_style_context_add_class(gtk_widget_get_style_context(db), "dock-btn");
        g_signal_connect(db, "clicked", apps[i].cb, NULL);
        gtk_box_pack_start(GTK_BOX(dock), db, FALSE, FALSE, 0);
    }
    gtk_box_pack_end(GTK_BOX(dv), dock, FALSE, FALSE, 14);

    gtk_stack_add_named(GTK_STACK(main_stack), dv, "desktop");
    gtk_stack_set_visible_child_name(GTK_STACK(main_stack), "loader");
    update_stats(NULL);
    g_timeout_add_seconds(3, update_stats, NULL);
    g_timeout_add(1100, end_load, NULL);
    gtk_widget_show_all(main_win);
    gtk_main();
    return 0;
}
