import processing.serial.*;
import java.io.*;

Serial port;
PrintWriter logWriter;
String mode = "";
int lineCount = 0;

void setup() {
  size(500, 200);
  println("Press # to select port. L=latency, N=noise, Q=quit.");
  String[] ports = Serial.list();
  for (int i = 0; i < ports.length; i++) {
    println(i + ": " + ports[i]);
  }
}

void draw() {
  background(0);
  fill(255);
  String p = (port != null) ? port.getPortName() : "none";
  text("port: " + p + " mode: " + mode + " lines: " + lineCount, 10, 20);
}

void keyPressed() {
  if (key == 'Q' || key == 'q') {
    quitLogger();
  } else if (key >= '0' && key <= '9' && port == null) {
    int idx = key - '0';
    String[] ports = Serial.list();
    if (idx < ports.length) {
      port = new Serial(this, ports[idx], 115200);
      port.bufferUntil('\n');
      println("Opened " + ports[idx]);
    }
  } else if (key == 'L' || key == 'l') {
    startLogging("docs/bench/latency/latency.csv", "latency");
  } else if (key == 'N' || key == 'n') {
    startLogging("docs/bench/noise/adc_idle.csv", "noise");
  }
}

void serialEvent(Serial p) {
  String line = p.readStringUntil('\n');
  if (line != null) {
    line = trim(line);
    println(line);
    if (logWriter != null) {
      logWriter.println(line);
      logWriter.flush();
      lineCount++;
    }
  }
}

void startLogging(String path, String m) {
  if (port == null) {
    println("No port selected.");
    return;
  }
  mode = m;
  try {
    File f = new File(path);
    f.getParentFile().mkdirs();
    logWriter = new PrintWriter(new FileWriter(f, true));
    println("Logging to " + f.getAbsolutePath());
  } catch (IOException e) {
    e.printStackTrace();
  }
}

void quitLogger() {
  if (logWriter != null) {
    logWriter.flush();
    logWriter.close();
  }
  if (port != null) {
    port.stop();
  }
  exit();
}
