#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <gtkmm.h>
#include <map>
#include <iostream>
#include <libtsm.h>
#include <cassert>

struct tsm_cell {
  uint32_t ch;
  struct tsm_screen_attr attr;
};

class RemoteWindow : public Gtk::Window
{
  private:
    Cairo::RefPtr<Cairo::Surface> surface;
    int fontsize;
    int spacing;
  public:
    RemoteWindow();
    virtual ~RemoteWindow();
    bool on_draw(const Cairo::RefPtr<Cairo::Context> &);
    void draw(struct tsm_cell *cells, int cols, int lines, int x, int y);
};

RemoteWindow::RemoteWindow() : spacing(2), fontsize(20)
{
}

RemoteWindow::~RemoteWindow()
{
}

bool RemoteWindow::on_draw(const Cairo::RefPtr<Cairo::Context> &cr)
{
  cr->set_source(surface, 0, 0);
  cr->paint();
  return TRUE;
}

void RemoteWindow::draw(struct tsm_cell *cells, int cols, int lines, int x, int y)
{
  int width = cols * fontsize / 2 + (cols + 1) * spacing / 2;
  int height = (lines + 1) * (fontsize + spacing);
  if (get_allocated_width () != width
  || get_allocated_height() != height)
  {
    resize(width, height);
    surface = get_window()->create_similar_surface(::Cairo::Content::CONTENT_COLOR,
                                                   width,
                                                   height);
  }

  Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(surface);
  cr->set_font_size (fontsize);
  for (int row = 0; row < lines; row++)
    for (int col = 0; col < cols; col++)
    {
      uint32_t ch = cells[col + row * cols].ch;
      width = tsm_ucs4_get_width(ch);
      char s[8];
      cr->set_source_rgb(cells[col + row * cols].attr.br,
                         cells[col + row * cols].attr.bg,
                         cells[col + row * cols].attr.bb);
      cr->rectangle (col * fontsize / 2 + col * spacing / 2,
                     row * (fontsize + spacing),
                     fontsize / 2 + spacing / 2 * (width ? width : 1),
                     fontsize + spacing);
      cr->fill ();
      if (width)
      {
        memset (s, 0, 8);
        tsm_ucs4_to_utf8 (ch, s);
        cr->set_source_rgb (cells[col + row * cols].attr.fr,
                            cells[col + row * cols].attr.fg,
                            cells[col + row * cols].attr.fb);
        cr->move_to (col * fontsize / 2 + (col + 1) * spacing / 2,
                     (row + 1 ) * (fontsize + spacing) - spacing * 2);
        cr->show_text (s);
        col += width - 1;
      }
    }
  width = tsm_ucs4_get_width(cells[x + y * cols].ch);
  cr->set_source_rgba (1.0, 1.0, 1.0, 0.8);
  cr->move_to (x * fontsize / 2 + (x + 1) * spacing / 2 * (width ? width : 1),
               (y + 1) * (fontsize + spacing) - spacing * 2);
  cr->show_text (width < 2 ? "▌" : "▉");
}

class MainWindow : public Gtk::Window
{
  private:
    struct nn_pollfd *fds;
    std::map<std::string, RemoteWindow *> windows;
    sigc::connection conn;
  public:
    MainWindow();
    virtual ~MainWindow();
    gboolean on_poll();
    bool on_delete_event(GdkEventAny *);
};

MainWindow::MainWindow()
{
  fds = (struct nn_pollfd *)malloc (sizeof (struct nn_pollfd));
  fds[0].fd = nn_socket (AF_SP, NN_PAIR);
  nn_connect (fds[0].fd, "tcp://192.168.2.40:7788");
  fds[0].events = NN_POLLIN;
  set_default_size(300, 200);
  set_border_width(2);

  conn = Glib::signal_idle().connect(sigc::mem_fun(*this, &MainWindow::on_poll));
}

MainWindow::~MainWindow()
{
  nn_close (fds[0].fd);
  free (fds);
  std::map<std::string, RemoteWindow *>::iterator iter; 
  for (iter = windows.begin(); iter != windows.end(); iter++)
  {
    delete iter->second;
  }
  windows.clear();
  conn.disconnect();
  printf ("destroyed.\n");
}

bool MainWindow::on_delete_event(GdkEventAny *)
{
  get_application()->quit();
  return TRUE;
}

gboolean MainWindow::on_poll()
{
  nn_poll (fds, 1, 0);
  void *msg = NULL;
  nn_recv (fds[0].fd, &msg, NN_MSG, 0);
  if (strncmp ((const char *)msg, "putong", 6) == 0)
  {
    std::cout << "putong" << std::endl;
  }
  else if (strncmp ((const char *)msg, "screen_on", 9) == 0)
  {
    std::cout << "screen on" << std::endl;
    char *end = strstr((char *)msg, "\n\n");
    *end = '\0';
    std::string mac = std::string(strstr((char *)msg, " ") + 1);
    char *screen = end + 2;
    end = strstr(screen, "\n");
    *end = '\0';
    struct tsm_cell *tsm_cells;
    tsm_cells = (struct tsm_cell *)(end + 1);
    
    RemoteWindow *w = windows[mac];
    if (w && !w->get_visible())
      w->show();
    else if (!w)
    {
      w = new RemoteWindow();
      w->set_title (mac);
      w->show();
      windows[mac] = w;
    }
    int cols, lines, pos_x, pos_y;
    std::stringstream(screen) >> cols >> lines >> pos_x >> pos_y;
    w->draw(tsm_cells, cols, lines, pos_x, pos_y);
    w->queue_draw();
  }
  else if (strncmp ((const char *)msg, "screen_off", 10) == 0)
  {
    std::cout << "screen off" << std::endl;
    char *mac = strstr((char *)msg, " ") + 1;
    if (windows[std::string(mac)])
      delete windows[std::string(mac)];
    windows.erase(std::string(mac));
  }
  else
  {
    std::cout << "invalid msg" << std::endl;
  }
  nn_freemsg (msg);
  return TRUE;
}

int main(int argc, char *argv[])
{
  auto app = Gtk::Application::create(argc, argv, "com.pi-clasroom.center");
  MainWindow w;
  return app->run(w);
}
