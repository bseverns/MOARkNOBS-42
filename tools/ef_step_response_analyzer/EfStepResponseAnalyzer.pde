import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;

// Analyze CSVs written by tools/ef_latency_logger/EfLatencyCsv.pde.
// A run starts with a meaningful MIDI-value movement and ends when direction
// reverses or samples pause. Its 10–90% duration is output-response timing,
// not analog-input-to-MIDI latency.
final int MIN_RESPONSE_AMPLITUDE = 16;
final int VALUE_NOISE_TOLERANCE = 1;
final long RUN_GAP_US = 100000;

ArrayList<ResponseRun> runs = new ArrayList<ResponseRun>();
String sourceName = "no CSV selected";
String status = "Press O to choose an EF latency CSV.";

class Sample {
  int efIndex;
  int slotIndex;
  int value;
  long emitUs;
  float deviceLatencyUs;

  Sample(int efIndex, int slotIndex, int value, long emitUs, float deviceLatencyUs) {
    this.efIndex = efIndex;
    this.slotIndex = slotIndex;
    this.value = value;
    this.emitUs = emitUs;
    this.deviceLatencyUs = deviceLatencyUs;
  }
}

class ResponseRun {
  int efIndex;
  int slotIndex;
  int direction;
  ArrayList<Sample> samples = new ArrayList<Sample>();

  ResponseRun(Sample first, int direction) {
    efIndex = first.efIndex;
    slotIndex = first.slotIndex;
    this.direction = direction;
    samples.add(first);
  }

  Sample first() { return samples.get(0); }
  Sample last() { return samples.get(samples.size() - 1); }

  int amplitude() { return abs(last().value - first().value); }

  long observedTenToNinetyUs() {
    int delta = last().value - first().value;
    if (abs(delta) < MIN_RESPONSE_AMPLITUDE) return -1;
    float low = first().value + delta * 0.1;
    float high = first().value + delta * 0.9;
    long lowUs = -1;
    long highUs = -1;
    for (Sample sample : samples) {
      boolean crossesLow = direction > 0 ? sample.value >= low : sample.value <= low;
      boolean crossesHigh = direction > 0 ? sample.value >= high : sample.value <= high;
      if (lowUs < 0 && crossesLow) lowUs = sample.emitUs;
      if (highUs < 0 && crossesHigh) {
        highUs = sample.emitUs;
        break;
      }
    }
    return lowUs >= 0 && highUs >= lowUs ? highUs - lowUs : -1;
  }

  float meanCadenceUs() {
    if (samples.size() < 2) return 0;
    return (last().emitUs - first().emitUs) / float(samples.size() - 1);
  }

  float meanDeviceLatencyUs() {
    float total = 0;
    for (Sample sample : samples) total += sample.deviceLatencyUs;
    return total / samples.size();
  }
}

void setup() {
  size(960, 520);
  textFont(createFont("Monospaced", 14));
}

void draw() {
  background(18);
  fill(230);
  text("EF Step Response Analyzer", 18, 30);
  fill(185);
  text("source: " + sourceName, 18, 56);
  text(status, 18, 82);
  text("O: open CSV    E: export summary    Q: quit", 18, 108);
  text("10–90% is inferred from MIDI-output changes; it excludes analog-front-end trigger time.", 18, 134);

  int y = 170;
  fill(230);
  text("EF / slot    direction  amplitude  samples  observed 10–90 us  cadence us  mean device us", 18, y);
  y += 24;
  fill(200);
  for (int index = 0; index < min(runs.size(), 14); index++) {
    ResponseRun run = runs.get(index);
    String direction = run.direction > 0 ? "rise   " : "release";
    String tenToNinety = run.observedTenToNinetyUs() < 0 ? "n/a" : str(run.observedTenToNinetyUs());
    String line = String.format("EF%-2d / %-3d  %-7s  %9d  %7d  %18s  %10.1f  %14.1f",
      run.efIndex, run.slotIndex, direction, run.amplitude(), run.samples.size(),
      tenToNinety, run.meanCadenceUs(), run.meanDeviceLatencyUs());
    text(line, 18, y);
    y += 22;
  }
  if (runs.size() > 14) text("… " + (runs.size() - 14) + " more runs; export includes all.", 18, y + 8);
}

void keyPressed() {
  if (key == 'o' || key == 'O') selectInput("Choose EF latency CSV", "loadCsv");
  if (key == 'e' || key == 'E') exportSummary();
  if (key == 'q' || key == 'Q') exit();
}

void loadCsv(File selected) {
  if (selected == null) return;
  sourceName = selected.getAbsolutePath();
  HashMap<String, ArrayList<Sample>> groups = new HashMap<String, ArrayList<Sample>>();
  String[] lines = loadStrings(selected);
  if (lines == null || lines.length < 2) {
    status = "No records found.";
    return;
  }
  String[] header = split(lines[0], ',');
  HashMap<String, Integer> column = new HashMap<String, Integer>();
  for (int index = 0; index < header.length; index++) column.put(trim(header[index]), index);
  String[] required = {"emit_us", "device_latency_us", "ef_index", "slot_index", "midi_value"};
  for (String name : required) {
    if (!column.containsKey(name)) {
      status = "Missing required CSV column: " + name;
      return;
    }
  }
  int accepted = 0;
  for (int lineIndex = 1; lineIndex < lines.length; lineIndex++) {
    String[] fields = split(lines[lineIndex], ',');
    if (fields.length <= maxColumn(column, required)) continue;
    try {
      int ef = Integer.parseInt(fields[column.get("ef_index")]);
      int slot = Integer.parseInt(fields[column.get("slot_index")]);
      int value = Integer.parseInt(fields[column.get("midi_value")]);
      long emitUs = Long.parseLong(fields[column.get("emit_us")]);
      float latency = Float.parseFloat(fields[column.get("device_latency_us")]);
      String key = ef + ":" + slot;
      if (!groups.containsKey(key)) groups.put(key, new ArrayList<Sample>());
      groups.get(key).add(new Sample(ef, slot, value, emitUs, latency));
      accepted++;
    } catch (RuntimeException ignored) {
      // Preserve valid records around a corrupt or manually edited line.
    }
  }
  runs.clear();
  for (ArrayList<Sample> samples : groups.values()) extractRuns(samples);
  status = "Loaded " + accepted + " EF records; inferred " + runs.size() + " response runs.";
}

int maxColumn(HashMap<String, Integer> column, String[] names) {
  int maximum = 0;
  for (String name : names) maximum = max(maximum, column.get(name));
  return maximum;
}

void extractRuns(ArrayList<Sample> samples) {
  if (samples.size() < 2) return;
  ResponseRun active = null;
  for (int index = 1; index < samples.size(); index++) {
    Sample previous = samples.get(index - 1);
    Sample current = samples.get(index);
    int difference = current.value - previous.value;
    int direction = difference > VALUE_NOISE_TOLERANCE ? 1 :
      difference < -VALUE_NOISE_TOLERANCE ? -1 : 0;
    boolean gap = current.emitUs - previous.emitUs > RUN_GAP_US;
    if (direction == 0) {
      if (active != null && !gap) active.samples.add(current);
      continue;
    }
    if (active == null || gap || active.direction != direction) {
      finishRun(active);
      active = new ResponseRun(previous, direction);
    }
    active.samples.add(current);
  }
  finishRun(active);
}

void finishRun(ResponseRun run) {
  if (run != null && run.samples.size() >= 2 && run.amplitude() >= MIN_RESPONSE_AMPLITUDE) runs.add(run);
}

void exportSummary() {
  if (runs.size() == 0) {
    status = "Nothing to export: load a CSV with response runs first.";
    return;
  }
  String stamp = new SimpleDateFormat("yyyyMMdd-HHmmss").format(new Date());
  File output = new File(sketchPath("data/ef-step-response-" + stamp + ".csv"));
  output.getParentFile().mkdirs();
  try {
    PrintWriter writer = new PrintWriter(new FileWriter(output));
    writer.println("ef_index,slot_index,direction,start_emit_us,end_emit_us,start_value,end_value,amplitude,samples,observed_10_90_us,mean_cadence_us,mean_device_latency_us");
    for (ResponseRun run : runs) {
      writer.println(run.efIndex + "," + run.slotIndex + "," +
        (run.direction > 0 ? "rise" : "release") + "," + run.first().emitUs + "," +
        run.last().emitUs + "," + run.first().value + "," + run.last().value + "," +
        run.amplitude() + "," + run.samples.size() + "," + run.observedTenToNinetyUs() + "," +
        nf(run.meanCadenceUs(), 0, 3) + "," + nf(run.meanDeviceLatencyUs(), 0, 3));
    }
    writer.flush();
    writer.close();
    status = "Exported " + runs.size() + " runs to " + output.getAbsolutePath();
  } catch (IOException error) {
    status = "Could not export summary: " + error.getMessage();
  }
}
