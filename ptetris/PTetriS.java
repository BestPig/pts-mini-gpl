/* PTetriS.java -- PTetriS v0.3, a really minimalistic Tetris clone
 * (C) <pts@fazekas.hu> in Early October 2001.
 *
 * This is a JDK1.1 applet and application, and does graphics with Java AWT.
 * See EOF for revision history.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Credits: color palette, geometry and brick shapes are modelled after
 * FUXOFT's Tetris2 (architecture: ZX/Spectrum).
 */

import java.applet.Applet;
import java.awt.Color;
import java.awt.Canvas;
import java.awt.Dimension;
import java.awt.Frame;
import java.awt.FlowLayout;
import java.awt.BorderLayout;
import java.awt.event.KeyListener;
import java.awt.event.KeyEvent;
import java.awt.event.FocusListener;
import java.awt.event.FocusEvent;
import java.awt.event.MouseListener;
import java.awt.event.MouseEvent;
import java.awt.event.WindowListener;
import java.awt.event.WindowEvent;
import java.awt.Graphics;

public class PTetriS extends Applet implements Runnable, KeyListener, FocusListener, MouseListener, WindowListener {
  // vvv these are the 1st column in VIM's syntax.txt
  //     color values are stolen from TETRIS2 for ZX/Spectrum
  static final Color black=new Color(0),
                     darkblue=new Color(0xbf), // ??
                     darkgreen=new Color(0xbf00), // ??
                     darkcyan=new Color(0xbfbf),
                     darkred=new Color(0xbf0000),
                     darkmagenta=new Color(0xbf00bf),
                     brown=new Color(0xbfbf00), // rather darkyellow
                     lightgray=new Color(0xbfbfbf),
                     darkgray=new Color(0x5f5f5f), // ??
                     blue=new Color(0xff), // ??
                     green=new Color(0xff00),
                     cyan=new Color(0xffff),
                     red=new Color(0xff0000), // ??
                     magenta=new Color(0xff00ff),
                     yellow=new Color(0xffff00),
                     white=new Color(0xffffff),
                     BG=black,
                     BORDER=darkmagenta,
                     MSG=white,
                     C[]={darkcyan,darkgreen,lightgray,cyan,yellow,green,brown};
  // sorry, no java classes for the shapes :-(
  static final String S[][]={
   {"......#.......#.",
    "####..#.####..#.",
    "......#.......#.",
    "......#.......#."},
   
   {"................",
    ".##..##..##..##.",
    ".##..##..##..##.",
    "................"},

   {"................",
    "..##.#....##.#..",
    ".##..##..##..##.",
    "......#.......#."},

   {"................",
    ".##...#..##...#.",
    "..##.##...##.##.",
    ".....#.......#.."},

   {"................",
    "..#...#.......#.",
    ".###.##..###..##",
    "......#...#...#."},

   {"................",
    ".#....#.......##",
    ".###..#..###..#.",
    ".....##....#..#."},

   {"................",
    "...#.##.......#.",
    ".###..#..###..#.",
    "......#..#....##"},
  };
  static final String titlee="PTetriS v0.3 (C) pts@fazekas.hu, GNU GPL";

  // vvv I'm not sure whether we require these, so I've included them. Maybe
  //     we won't be able to get focus if we don't have these.
  public void focusGained(FocusEvent e) {}
  public void focusLost(FocusEvent e) {}

  public void windowActivated(WindowEvent e) { /*requestFocus();*/ } /*useless*/
  public void windowDeactivated(WindowEvent e) {} // stop(); comment OK
  public void windowClosed(WindowEvent e) {}
  public void windowClosing(WindowEvent e) {
    ((Frame)e.getComponent()).setVisible(false);
    destroy();
  }
  public void windowOpened(WindowEvent e) { requestFocus(); }
  public void windowIconified(WindowEvent e) { stop(); }
  public void windowDeiconified(WindowEvent e) { start(); } // BRR, Linux

  // vvv these MouseEvent handlers are _required_ for JDK1.1+appletviewer,
  //     because we can get KeyEvents only if we've already got the keyboard
  //     focus, and we don't have focus by default :-(( 
  public void mouseClicked(MouseEvent e) {
    requestFocus();
    th_playing=!th_playing;
    // System.out.println("Klikk.");
    try { synchronized(this) { notify(); } } catch (Exception ee) {}
  }
  public void mouseReleased(MouseEvent e) {}
  public void mousePressed(MouseEvent e) {}
  public void mouseEntered(MouseEvent e) { requestFocus(); }
  public void mouseExited(MouseEvent e) {}
  
  // vvv no need for an extra Canvas, we'll use our Applet.
  // static class Palya extends Canvas {
  static final int WDB=14, HTB=24, // pályaméret, 2 vastag kerettel
                   BS=20, // block size, in pixels
                   WD=WDB*BS, HT=HTB*BS;
  static final Dimension d=new Dimension(WD,HT);
  public void setSize(int w, int h) { super.setSize(WD,HT); }
  public void setSize(Dimension d) { super.setSize(WD,HT); }
  public Dimension getPreferredSize() { return d; }
  public Dimension getMinimumSize() { return d; }
  public Dimension getMaximumSize() { return d; }
  // vvv for Java version 1.1.7 compatibility
  static final int VK_KP_UP=0xE0, VK_KP_DOWN=0xE1, VK_KP_LEFT=0xE2, VK_KP_RIGHT=0xE3;

  long releaseWhen=0L;

  public void keyPressed(KeyEvent e) {
    //System.out.println(e.getKeyCode());
    //System.out.println(e.getKeyChar());
    if (!th_playing) return;
    char c=Character.toUpperCase(e.getKeyChar());
    int kc=e.getKeyCode();
    if (kc==VK_KP_UP || kc==e.VK_UP) c='R';
    else if (kc==VK_KP_DOWN || kc==e.VK_DOWN ) c='F';
    else if (kc==VK_KP_LEFT || kc==e.VK_LEFT ) c='D';
    else if (kc==VK_KP_RIGHT|| kc==e.VK_RIGHT) c='G';
    if (c=='F' && no_fast_downfall && releaseWhen==e.getWhen()) c='X';
    if (c=='R' || c=='D' || c=='F' || c=='G') {
      no_fast_downfall=false;
      synchronized(t) {
        int sbx=bx, sba=ba, sby=by;
        putBrick(BG);
        if (c=='R') ba=(ba+1)%4;
        else if (c=='D') bx--;
        else if (c=='F') by++;
        else if (c=='G') bx++;
        if (!isBrickOK()) { bx=sbx; ba=sba; by=sby; }
        putBrick(C[bt]);
      }
      repaint();
    }
  }

  public void keyReleased(KeyEvent e) {
    // no_fast_downfall=false;
    // System.out.println();
    releaseWhen=e.getWhen();
  }
  public void keyTyped(KeyEvent e) { }
  
  Color t[][]; // t[y][x]
  /**
   * x,y block offset of current brick from the top-left
   */
  int bx, by;
  /**
   * type of current brick: 0..6
   */
  int bt;
  /**
   * angle of current brick: 0..3
   */
  int ba;
  Thread th;
  boolean th_to_stop;
  boolean th_playing;
  boolean no_fast_downfall=false;

  /**
   * Queries current glyph validity.
   * @return true iff current glyph is valid, i.e no collissions
   */
  boolean isBrickOK() {
    String s[]=S[bt];
    int x, y;
    for (y=0;y<4;y++) for (x=0;x<4;x++) {
      if (s[y].charAt(x+ba*4)!='.' && t[y+by][x+bx]!=BG) return false;
    }
    return true;
  }

  /**
   * Puts the current brick into <code>this.t</code>.
   * @param c uses the specified color.
   */
  void putBrick(Color c) {
    String s[]=S[bt];
    int x, y;
    for (y=0;y<4;y++) for (x=0;x<4;x++) {
      if (s[y].charAt(x+ba*4)!='.') t[y+by][x+bx]=c;
    }
  }

  void newBrick() {
    bt=(int)(Math.random()*7);
    by=-1+2; bx=3+2; ba=0;
  }
  
  void newGame() {
    int x, y;
    Color u[];
    t=new Color[HTB+2][];
    for (y=0;y<HTB;y++) {
      t[y]=u=new Color[WDB+2];
      u[0]=u[1]=u[WDB-2]=u[WDB-1]=BORDER;
      for (x=2;x<WDB-2;x++) u[x]=BG;
    }
    for (x=2;x<WDB-2;x++) t[0][x]=t[1][x]=t[HTB-2][x]=t[HTB-1][x]=BORDER;
    newBrick();
    putBrick(C[bt]);
  }

  void removeFullLines() {
    int x, yup=HTB-3, ydown=HTB-3;
    boolean hole;
    while (true) {
      for (hole=false;!hole && yup>=2;yup--) {
        for (x=2;x<WDB-2;x++) if (t[yup][x]==BG) { hole=true; break; }
      }
      if (!hole) break;
      for (x=2;x<WDB-2;x++) t[ydown][x]=t[yup+1][x];
      ydown--;
    }
    for (;ydown>=2;ydown--) for (x=2;x<WDB-2;x++) t[ydown][x]=BG;
  }

  /**
   * This is ugly, slow, and not double-buffered. Those goals are beyond the
   * scope of this code. (Maybe beyond the scope of efficient Java??)
   */
  public void paint(Graphics g) {
    int x, y;
    Color u[];
    for (y=0;y<HTB;y++) { u=t[y];
      for (x=0;x<WDB;x++) {
        g.setColor(u[x]);
        g.fillRect(x*BS,y*BS,BS,BS);
      }
    }
  }
  public void update(Graphics g) { paint(g); }


  //Palya p;
  public void init() {
    //System.out.println("Hi!");
    setSize(WD,HT);
    setBackground(green);
    // setResizable(false); // undefined method
    //setLayout(new FlowLayout(FlowLayout.CENTER, 0, 0));
    setLayout(new BorderLayout(0,0));
    requestFocus(); /*useless*/
    addKeyListener(this);
    addFocusListener(this);
    addMouseListener(this);
    requestFocus(); /*useless*/
    //add(p=new Palya());
    th=new Thread(this); // timer+game logic thread
    th.setDaemon(true); // so it won't prevent the process from exiting
    newGame();
    th.start();
  }

  boolean n1ss;
  public void start() {
    requestFocus(); /*useless*/
    if (n1ss) {
      th_playing=true;
      try { synchronized(this) { notify(); } } catch (Exception ee) {}
    }
    n1ss=true;
  }
  public void stop() {
    th_playing=false;
  }

  public boolean running_as_application;
  public void destroy() {
    try { th_to_stop=true; th.interrupt(); } catch (Exception e) {}
    if (running_as_application) System.out.println("Bye!");
    if (running_as_application) System.exit(0);
  }

  /**
   * This gets run in Thread <code>this.th</code>.
   */
  public void run() {
    boolean have_game=true;
    while (!th_to_stop) {
      try { synchronized(this) {
        if (!th_playing) {
          wait(); // the monitor is the Applet object itself.
          // System.out.println("Woken up."); // DEBUG
          continue;
        }
      } } catch(InterruptedException e) { continue; }
      // System.out.println("Time is going"); // DEBUG
      synchronized (t) {
        if (!have_game) {
          newGame();
          have_game=true;
        } else {
          putBrick(BG); by++;
          if (isBrickOK()) {
            putBrick(C[bt]);
          } else {
            // brick reached its final position
            no_fast_downfall=true; // next brick will begin falling slowly
            by--; putBrick(C[bt]);
            removeFullLines();
            newBrick();
            if (isBrickOK()) {
              putBrick(C[bt]);
            } else {
              if (running_as_application)
                System.out.println("Game Over. Click on the window for a new game.");
              have_game=false;
              th_playing=false;
            }
          }
        }
      }
      repaint();
      try { Thread.sleep(300); } catch (InterruptedException e) {} //millis
    }
  }
  /**
   * Only for the Application version.
   */
  static class PtsFrame extends Frame {
    static final Dimension d=new Dimension(WD,HT);

// BUG01 fixed at Fri Dec 28 15:01:03 CET 2001
// Advice: never ever override these methods of Frame/Window:
//    public void setSize(int w, int h) {}
//    public void setSize(Dimension d) {}
//    public Dimension getPreferredSize() { return d; }
//    public Dimension getMinimumSize() { return d; }
//    public Dimension getMaximumSize() { return d; }

    public PtsFrame() {
      setBackground(magenta);
      setTitle(titlee);
      //super.setSize(d);
      // ^^^ BUG01 fixed. Advice: never-ever call Frame/Window#setSize
      setResizable(false);
    }
  }

  /**
   * Only for the Application version.
   */
  public static void main(String argv[]) {
    System.out.println("This is "+titlee);
    System.out.println(
      "http://borsodi.iit.bme.hu/entitanok/ptetris/\n"+
      "THIS SOFTWARE COMES WITH ABSOLUTELY NO WARRANTY! USE AT YOUR OWN RISK!\n"+
      "This program is free software, covered by the GNU GPL.\n"+
      "\n"+
      "Please resize window if it's too small!\n"+
      "Click on the window to start a new game.\n"+
      "Click on the window to pause/continue current game.\n"+
      "Close the window to exit from the program.\n"+
      "Use the keys D, F, G, R or the arrow keys to control the falling brick.\n"+
      "The goal of the game is survival.\n"+
      "If you create a row without holes, it disappears immediately.\n"
    );
    PtsFrame f=new PtsFrame();
    PTetriS p=new PTetriS();
    p.running_as_application=true;
    // vvv only FlowLayout can guarantee that PTetriS won't get resized
    //f.setLayout(new BorderLayout(0,0)); // bad!!
    f.addWindowListener(p);
    f.add(p);
    f.doLayout(); /*useless*/
    p.init();
    f.pack();
    // ^^^ helps (solves) the Linux/X11/JDK1.1 problem (BUG01)
    //     Advice: just put a Component with the proper size inside
    //     Window/Frame, and then call Frame#pack(). Never ever call
    //     Frame#setSize() or Frame#size(), never ever override
    //     Frame#get(preferred|minimum|maximum)Size. You may call
    //     Frame#setLayout and Frame#show after this.
    f.setLayout(new FlowLayout(FlowLayout.CENTER, 0, 0));
    f.show();
    p.start();
  }
}

/* Revision history (CHANGES, ChangeLog):
 *
 * v0.1 Tue Oct  2 19:58:32 CEST 2001 -- Tue Oct  2 23:30:29 CEST 2001
 *      (3.5 hours of work) playable version, HTML, applet, application
 * v0.2 Tue Oct  2 23:37:58 CEST 2001
 *      Dedicated to Rév Szilvia, added more docs, added licensing, added
 *      help at application startup. Source converted to HTML etc.
 * v0.3 Fri Dec 28 15:07:02 CET 2001
 *      BUG01 solved at last. Slower falls. Faster downfall cancelled for
 *      the next brick.
 */
