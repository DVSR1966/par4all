/*
 $Id$
 
 Send displays to emacs.
 */

package fr.ensmp.cri.jpips;

import java.io.*;

public class EmacsDisplayer 
  extends Displayer
{
  static public final String BEGIN = "\200";
  static public final String END = "\201";
  public Process g2davinci; 
  public Process davinci; 
  PrintStream to_emacs;
  
  public EmacsDisplayer(PrintStream out)
  {
    this.to_emacs = out;
  }
  
  boolean display(File file, boolean locked, boolean writable)
  {
    to_emacs.print(BEGIN + "WINDOW_NUMBER:" + 4 + END +
                   BEGIN + "Sequential View:" + file + END);
    to_emacs.flush();
    return true;
  }
  
  void display(String name, String string, boolean locked, boolean writable)
  {
    display(new File(name), locked, writable);
  }
  
  boolean graphdisplay(File file, boolean locked, boolean writable)
  {
    String filename = file.getPath();
    try
    {
      String ps = "pips_graph2daVinci -launch_daVinci " + filename;
      System.err.println("running: " + ps);
      g2davinci = Runtime.getRuntime().exec(ps);
      System.err.println("done");
    }
    catch (Exception e)
    {
      System.err.println("EmacsDisplayer.graphdisplay Exception :" + e);
    }
    
    /*
     int index = filename.lastIndexOf('-');
     String ft = filename.substring(1,index);
     String davinciname = ft + "-daVinci";
     String pstring1 =  "daVinci " + davinciname;
     try
     {
     davinci = Runtime.getRuntime().exec(pstring1);
     }
     catch (Exception e)
     {
     System.err.println("EmacsDisplayer.graphdisplay Exception :" + e);
     }
     */      
    
    //to_emacs.print(BEGIN + "VIEW_DAVINCI_GRAPH:" + filename + END);
    //to_emacs.flush(); 
    
    return true;
  }
}
