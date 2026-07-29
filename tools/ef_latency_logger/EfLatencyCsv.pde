import processing.serial.Serial;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.text.SimpleDateFormat;
import java.util.Date;

// Captures BENCH_EF_LATENCY_LOG records from teensy40_ef_latency_bench.
// `device_latency_us` is device-local: last digital EF sample to MIDI enqueue.
// Host receipt time is intentionally kept separate because its clock is not
// synchronized with the Teensy and cannot prove analog-to-host latency.
Serial port;
PrintWriter csv;
String portName = "none";
String csvPath = "not recording";
int records = 0;
float minLatencyUs = Float.POSITIVE_INFINITY;
float maxLatencyUs = 0;
double sumLatencyUs = 0;

void setup() {
  size(760, 220);
  textFont(createFont("Monospaced", 14));
  println("EF latency logger: press a digit to open that 115200 serial port; R starts CSV; Q quits.");
  String[] ports = Serial.list();
  for (int index = 0; index < ports.length; index++) println(index + ": " + ports[index]);
}

void draw() {
  background(18);
  fill(230);
  text("port: " + portName, 16, 32);
  text("csv: " + csvPath, 16, 58);
  text("EF records: " + records, 16, 84);
  String stats = records == 0 ? "waiting for ef_latency records" :
    String.format("device sample-to-enqueue: mean %.1f us, min %.1f us, max %.1f us",
                  sumLatencyUs / records, minLatencyUs, maxLatencyUs);
  text(stats, 16, 110);
  fill(165);
  text("This is firmware processing timing. Use a shared trigger/scope for analog input-to-MIDI arrival.", 16, 156);
  text("R records to data/ef-latency-<timestamp>.csv. Press Q to close safely.", 16, 184);
}

void keyPressed() {
  if ((key == 'q' || key == 'Q')) {
    closeLogger();
    return;
  }
  if (key == 'r' || key == 'R') {
    startCsv();
    return;
  }
  if (port == null && key >= '0' && key <= '9') {
    int index = key - '0';
    String[] ports = Serial.list();
    if (index >= ports.length) return;
    port = new Serial(this, ports[index], 115200);
    port.bufferUntil('\n');
    portName = ports[index];
    println("Opened " + portName);
  }
}

void serialEvent(Serial source) {
  String line = source.readStringUntil('\n');
  if (line == null) return;
  line = trim(line);
  if (!line.startsWith("ef_latency,")) return;

  String[] fields = split(line, ',');
  if (fields.length != 10) {
    println("Ignoring malformed EF latency record: " + line);
    return;
  }
  float latencyUs;
  try {
    latencyUs = Float.parseFloat(fields[4]);
  } catch (RuntimeException error) {
    println("Ignoring EF latency with invalid device_latency_us: " + line);
    return;
  }

  records++;
  minLatencyUs = min(minLatencyUs, latencyUs);
  maxLatencyUs = max(maxLatencyUs, latencyUs);
  sumLatencyUs += latencyUs;
  println(line);
  if (csv != null) {
    csv.println(nf(millis(), 0, 3) + "," + line);
    csv.flush();
  }
}

void startCsv() {
  if (port == null) {
    println("Select a serial port first.");
    return;
  }
  if (csv != null) return;
  String stamp = new SimpleDateFormat("yyyyMMdd-HHmmss").format(new Date());
  File output = new File(sketchPath("data/ef-latency-" + stamp + ".csv"));
  output.getParentFile().mkdirs();
  try {
    csv = new PrintWriter(new FileWriter(output));
    csv.println("host_receive_ms,record,device_ms,source_us,emit_us,device_latency_us,ef_index,slot_index,ef_level,midi_value,path");
    csvPath = output.getAbsolutePath();
    println("Recording " + csvPath);
  } catch (IOException error) {
    println("Could not open CSV: " + error.getMessage());
  }
}

void closeLogger() {
  if (csv != null) {
    csv.flush();
    csv.close();
  }
  if (port != null) port.stop();
  exit();
}
