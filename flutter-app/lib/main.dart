import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'filters/signal_filters.dart';
void main() {
  runApp(const InterruptZeroApp());
}

// ================================================================
// These MUST match the ESP32 firmware exactly (MYOSA_6.0_fixed.ino):
//   BLE_SERVICE_UUID / BLE_TELEM_CHAR_UUID / BLE_CRASH_CHAR_UUID /
//   BLE_PREBUF_CHAR_UUID
// ================================================================
final Guid kServiceUuid   = Guid('6e400001-b5a3-f393-e0a9-e50e24dcca9e');
final Guid kTelemCharUuid = Guid('6e400002-b5a3-f393-e0a9-e50e24dcca9e');
final Guid kCrashCharUuid = Guid('6e400003-b5a3-f393-e0a9-e50e24dcca9e');
final Guid kPrebufCharUuid = Guid('6e400004-b5a3-f393-e0a9-e50e24dcca9e');

class InterruptZeroApp extends StatelessWidget {
  const InterruptZeroApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Interrupt Zero',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        useMaterial3: true,
        colorScheme: ColorScheme.fromSeed(
          seedColor: Colors.cyanAccent,
          brightness: Brightness.dark,
        ),
      ),
      home: const WelcomeScreen(),
    );
  }
}

// ================================================================
// WELCOME SCREEN
// ================================================================
class WelcomeScreen extends StatelessWidget {
  const WelcomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      body: SafeArea(
        child: Column(
          children: [
            const SizedBox(height: 30),
            const Text(
              'MYOSA 6.0',
              style: TextStyle(
                color: Colors.cyanAccent,
                fontSize: 18,
                fontWeight: FontWeight.bold,
                letterSpacing: 3,
              ),
            ),
            const SizedBox(height: 10),
            const Text(
              'INTERRUPT ZERO',
              style: TextStyle(
                fontSize: 28,
                fontWeight: FontWeight.w900,
                letterSpacing: 1.5,
              ),
            ),
            const SizedBox(height: 20),
            Expanded(
              child: Padding(
                padding: const EdgeInsets.symmetric(horizontal: 20),
                child: Image.asset(
                  'assets/images/interrupt_zero_logo.png',
                  fit: BoxFit.contain,
                ),
              ),
            ),
            Padding(
              padding: const EdgeInsets.all(24),
              child: SizedBox(
                width: double.infinity,
                height: 55,
                child: ElevatedButton.icon(
                  icon: const Icon(Icons.bluetooth_searching),
                  label: const Text(
                    'START MONITORING',
                    style: TextStyle(
                      fontWeight: FontWeight.bold,
                      fontSize: 16,
                      letterSpacing: 1,
                    ),
                  ),
                  onPressed: () {
                    Navigator.push(
                      context,
                      MaterialPageRoute(
                        builder: (_) => const BluetoothScanScreen(),
                      ),
                    );
                  },
                ),
              ),
            ),
            const Text(
              'EDGE COMPUTE • SMARTER ROADS • SAFER DRIVES',
              style: TextStyle(
                color: Colors.white54,
                fontSize: 10,
                letterSpacing: 1.5,
              ),
            ),
            const SizedBox(height: 20),
          ],
        ),
      ),
    );
  }
}

// ================================================================
// BLUETOOTH SCAN SCREEN
// ================================================================
class BluetoothScanScreen extends StatefulWidget {
  const BluetoothScanScreen({super.key});

  @override
  State<BluetoothScanScreen> createState() => _BluetoothScanScreenState();
}

class _BluetoothScanScreenState extends State<BluetoothScanScreen> {
  StreamSubscription<List<ScanResult>>? _scanSubscription;

  List<ScanResult> _devices = [];
  bool _isScanning = false;
  String _status = 'Ready to scan';

  @override
  void initState() {
    super.initState();
    _startBle();
  }

  Future<void> _startBle() async {
    final permissions = await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
    ].request();

    if (!permissions.values.every((p) => p.isGranted)) {
      setState(() {
        _status = 'Bluetooth permission denied';
      });
      return;
    }

    await _startScan();
  }

  Future<void> _startScan() async {
    if (await FlutterBluePlus.isSupported == false) {
      setState(() {
        _status = 'BLE is not supported on this device';
      });
      return;
    }

    await FlutterBluePlus.adapterState
        .where((state) => state == BluetoothAdapterState.on)
        .first;

    _scanSubscription?.cancel();

    _scanSubscription = FlutterBluePlus.scanResults.listen((results) {
      if (!mounted) return;
      setState(() {
        _devices = results;
      });
    });

    setState(() {
      _isScanning = true;
      _status = 'Scanning for MYOSA / ESP32...';
    });

    try {
      // Filter by our own service UUID so we only see the MYOSA kit,
      // not every random BLE device nearby.
      await FlutterBluePlus.startScan(
  timeout: const Duration(seconds: 15),
);
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _status = 'Scan error: $e';
      });
    }

    if (!mounted) return;

    setState(() {
      _isScanning = false;
      _status = _devices.isEmpty
          ? 'No MYOSA devices found'
          : '${_devices.length} device(s) found';
    });
  }

  Future<void> _connect(ScanResult result) async {
  debugPrint(
    'CONNECTING TO: '
    '${result.device.platformName} '
    '${result.device.remoteId}',
  );

  setState(() {
    _status =
        'Connecting to ${result.device.platformName}...';
  });

  try {
    await FlutterBluePlus.stopScan();

    await result.device.connect(
      license: License.free,
      timeout: const Duration(seconds: 15),
    );

    if (!mounted) return;

    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (_) => BleMonitorScreen(
          device: result.device,
        ),
      ),
    );
  } catch (e) {
    if (!mounted) return;

    setState(() {
      _status = 'Connection error: $e';
    });
  }
}

  @override
  void dispose() {
    _scanSubscription?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        backgroundColor: Colors.black,
        title: const Text('CONNECT TO MYOSA'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: _isScanning ? null : _startScan,
          ),
        ],
      ),
      body: Column(
        children: [
          const SizedBox(height: 20),
          Icon(
            _isScanning ? Icons.bluetooth_searching : Icons.bluetooth,
            color: Colors.cyanAccent,
            size: 60,
          ),
          const SizedBox(height: 12),
          Text(
            _status,
            style: const TextStyle(color: Colors.white70),
          ),
          const SizedBox(height: 20),
          Expanded(
            child: _devices.isEmpty && !_isScanning
                ? const Center(
                    child: Text(
                      'Make sure the MYOSA ESP32 is powered on\nand advertising via BLE.',
                      textAlign: TextAlign.center,
                      style: TextStyle(color: Colors.white54),
                    ),
                  )
                : ListView.builder(
                    itemCount: _devices.length,
                    itemBuilder: (context, index) {
                      final result = _devices[index];

                      final name = result.device.platformName.isNotEmpty
                          ? result.device.platformName
                          : result.advertisementData.advName.isNotEmpty
                              ? result.advertisementData.advName
                              : 'Unknown BLE Device';

                      return Card(
                        color: const Color(0xFF101820),
                        margin: const EdgeInsets.symmetric(
                          horizontal: 16,
                          vertical: 6,
                        ),
                        child: ListTile(
                          leading: const Icon(
                            Icons.memory,
                            color: Colors.cyanAccent,
                          ),
                          title: Text(name),
                          subtitle: Text(
                            '${result.device.remoteId}\nRSSI: ${result.rssi}',
                          ),
                          isThreeLine: true,
                          trailing: const Icon(Icons.chevron_right),
                          onTap: () => _connect(result),
                        ),
                      );
                    },
                  ),
          ),
        ],
      ),
    );
  }
}

// ================================================================
// LIVE DASHBOARD — talks to the actual custom GATT service defined
// in MYOSA_6.0_fixed.ino: one JSON telemetry characteristic, one
// JSON crash-event characteristic, and one framed (BEGIN/lines/END)
// pre-impact buffer characteristic. No raw float arrays, no guessed
// UUID suffixes — this matches the firmware exactly.
// ================================================================
class BleMonitorScreen extends StatefulWidget {
  final BluetoothDevice device;

  const BleMonitorScreen({
    super.key,
    required this.device,
  });

  @override
  State<BleMonitorScreen> createState() => _BleMonitorScreenState();
}
  // ================================================================
  // SIGNAL FILTERS
  // ESP32 remains unchanged.
  // Filtering is performed only after BLE data reaches Flutter.
  // ================================================================

  final LowPassFilter _accelFilter =
      LowPassFilter(alpha: 0.15);

  final LowPassFilter _pressureFilter =
      LowPassFilter(alpha: 0.20);

  final MovingAverageFilter _dPdtFilter =
      MovingAverageFilter(windowSize: 5);
class PreBufferSample {
  final int tMs;
  final double accelMagG;
  final int pressurePa;
  final double dPdtPaS;

  PreBufferSample({
    required this.tMs,
    required this.accelMagG,
    required this.pressurePa,
    required this.dPdtPaS,
  });
}

class _BleMonitorScreenState extends State<BleMonitorScreen> {
  final List<StreamSubscription<List<int>>> _subscriptions = [];

  String _status = 'Discovering services...';
  int _packetCount = 0;

  // ---- Telemetry (from BLE_TELEM_CHAR_UUID, JSON) ----
  // Raw values received directly from ESP32
double? _rawAccelMagG;
int? _rawPressurePa;
double? _rawDPdt;

// Filtered values
double? _accelMagG;
int? _pressurePa;
double? _dPdt;

double? _pitchDeg;
double? _gyroXDps;
double? _tempC;
  int? _r, _g, _b;
  bool _crashActive = false;

  // ---- Crash event (from BLE_CRASH_CHAR_UUID, JSON) ----
  double? _peakG;
  int? _peakPressurePa;
  DateTime? _crashEventTime;

  // ---- Pre-impact buffer (from BLE_PREBUF_CHAR_UUID, framed CSV) ----
  final List<PreBufferSample> _preBuffer = [];
  bool _prebufStreaming = false;
  int? _prebufExpectedCount;

  @override
  void initState() {
    super.initState();
    _discoverAndListen();
  }

  Future<void> _discoverAndListen() async {
    try {
      final services = await widget.device.discoverServices();
      debugPrint('========== BLE SERVICES ==========');

for (final service in services) {
  debugPrint('SERVICE: ${service.uuid}');

  for (final characteristic in service.characteristics) {
    debugPrint(
      '  CHARACTERISTIC: ${characteristic.uuid} '
      'notify=${characteristic.properties.notify} '
      'indicate=${characteristic.properties.indicate} '
      'read=${characteristic.properties.read}',
    );
  }
}

debugPrint('==================================');

      final myosaService = services.firstWhere(
        (s) => s.uuid == kServiceUuid,
        orElse: () => throw Exception(
            'MYOSA custom service not found on this device'),
      );

      for (final characteristic in myosaService.characteristics) {
        if (!(characteristic.properties.notify ||
            characteristic.properties.indicate)) {
          continue;
        }

        final sub = characteristic.onValueReceived.listen((value) {
          _handleData(characteristic.uuid, value);
        });

        _subscriptions.add(sub);
        await characteristic.setNotifyValue(true);
      }

      if (!mounted) return;
      setState(() {
        _status = 'Connected • Listening';
      });
    } catch (e) {
      if (!mounted) return;
      setState(() => _status = 'BLE error: $e');
    }
  }

  void _handleData(Guid charUuid, List<int> value) {
    final text = utf8.decode(value, allowMalformed: true);

    if (charUuid == kTelemCharUuid) {
      _handleTelemetry(text);
    } else if (charUuid == kCrashCharUuid) {
      _handleCrash(text);
    } else if (charUuid == kPrebufCharUuid) {
      _handlePrebufLine(text);
    }
  }

  void _handleTelemetry(String jsonText) {
  try {
    final map = jsonDecode(jsonText) as Map<String, dynamic>;

    final rawG = (map['G'] as num?)?.toDouble();
    final rawPressure = (map['pressPa'] as num?)?.toInt();
    final rawDPdt = (map['dPdt'] as num?)?.toDouble();

    if (!mounted) return;

    setState(() {
      // ============================================================
      // RAW DATA — exactly as received from ESP32
      // ============================================================

      _rawAccelMagG = rawG;
      _rawPressurePa = rawPressure;
      _rawDPdt = rawDPdt;

      // ============================================================
      // FILTERED DATA
      // ============================================================

      if (rawG != null) {
        _accelMagG = _accelFilter.filter(rawG);
      }

      if (rawPressure != null) {
        final filteredPressure =
            _pressureFilter.filter(rawPressure.toDouble());

        _pressurePa = filteredPressure.round();
      }

      if (rawDPdt != null) {
        _dPdt = _dPdtFilter.filter(rawDPdt);
      }

      // ============================================================
      // Other ESP32 values — unchanged
      // ============================================================

      _pitchDeg =
          (map['pitch'] as num?)?.toDouble();

      _gyroXDps =
          (map['gyroX'] as num?)?.toDouble();

      _tempC =
          (map['tempC'] as num?)?.toDouble();

      _r =
          (map['r'] as num?)?.toInt();

      _g =
          (map['g'] as num?)?.toInt();

      _b =
          (map['b'] as num?)?.toInt();

      _crashActive =
          (map['crash'] as num?)?.toInt() == 1;

      _packetCount++;
    });
  } catch (_) {
    // Ignore malformed / partial BLE packets.
  }
}

  void _handleCrash(String jsonText) {
    try {
      final map = jsonDecode(jsonText) as Map<String, dynamic>;
      if (!mounted) return;
      setState(() {
        _peakG = (map['peakG'] as num?)?.toDouble();
        _peakPressurePa = (map['peakPressPa'] as num?)?.toInt();
        _crashEventTime = DateTime.now();
        _preBuffer.clear();
        _prebufStreaming = false;
        _prebufExpectedCount = null;
      });

      // Optional: pop a confirmation dialog / play a sound / vibrate here.
    } catch (_) {
      // ignore malformed crash payload
    }
  }

  void _handlePrebufLine(String line) {
    line = line.trim();
    if (line.isEmpty) return;

    if (line.startsWith('BEGIN')) {
      final parts = line.split(',');
      final count = parts.length > 1 ? int.tryParse(parts[1]) : null;
      if (!mounted) return;
      setState(() {
        _preBuffer.clear();
        _prebufStreaming = true;
        _prebufExpectedCount = count;
      });
      return;
    }

    if (line == 'END') {
      if (!mounted) return;
      setState(() {
        _prebufStreaming = false;
      });
      return;
    }

    // Otherwise: "t,accelMagG,pressurePa,dPdt"
    final fields = line.split(',');
    if (fields.length != 4) return;

    final t = int.tryParse(fields[0]);
    final accel = double.tryParse(fields[1]);
    final press = int.tryParse(fields[2]);
    final dpdt = double.tryParse(fields[3]);
    if (t == null || accel == null || press == null || dpdt == null) return;

    if (!mounted) return;
    setState(() {
      _preBuffer.add(PreBufferSample(
        tMs: t,
        accelMagG: accel,
        pressurePa: press,
        dPdtPaS: dpdt,
      ));
    });
  }

  @override
  void dispose() {
    for (final sub in _subscriptions) {
      sub.cancel();
    }
    widget.device.disconnect();
    super.dispose();
  }

  Widget _sectionCard(String title, List<Widget> rows) {
    return Container(
      margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 6),
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: const Color(0xFF101820),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: Colors.white12),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            title,
            style: const TextStyle(
              color: Colors.cyanAccent,
              fontWeight: FontWeight.bold,
              fontSize: 13,
              letterSpacing: 1.2,
            ),
          ),
          const SizedBox(height: 8),
          ...rows,
        ],
      ),
    );
  }

  Widget _row(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 3),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: const TextStyle(color: Colors.white60, fontSize: 13)),
          Text(
            value,
            style: const TextStyle(
              color: Colors.white,
              fontSize: 13,
              fontFamily: 'monospace',
              fontWeight: FontWeight.w600,
            ),
          ),
        ],
      ),
    );
  }

  String _fmt(num? v, {String unit = '', int decimals = 2}) {
    if (v == null) return '—';
    return '${v.toStringAsFixed(decimals)}$unit';
  }

  @override
  Widget build(BuildContext context) {
    final deviceName = widget.device.platformName.isNotEmpty
        ? widget.device.platformName
        : 'MYOSA ESP32';

    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        backgroundColor: Colors.black,
        title: const Text('LIVE DASHBOARD'),
      ),
      body: Column(
        children: [
          Container(
            width: double.infinity,
            margin: const EdgeInsets.all(16),
            padding: const EdgeInsets.all(16),
            decoration: BoxDecoration(
              color: const Color(0xFF101820),
              borderRadius: BorderRadius.circular(16),
              border: Border.all(
                color: _crashActive ? Colors.redAccent : Colors.cyanAccent,
              ),
            ),
            child: Column(
              children: [
                Icon(
                  _crashActive
                      ? Icons.warning_amber_rounded
                      : Icons.bluetooth_connected,
                  color: _crashActive ? Colors.redAccent : Colors.cyanAccent,
                  size: 32,
                ),
                const SizedBox(height: 6),
                Text(deviceName,
                    style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 16)),
                const SizedBox(height: 4),
                Text(_status,
                    textAlign: TextAlign.center,
                    style: const TextStyle(color: Colors.white70, fontSize: 12)),
                Text('Packets received: $_packetCount',
                    style: const TextStyle(color: Colors.white38, fontSize: 11)),
                if (_crashActive)
                  const Padding(
                    padding: EdgeInsets.only(top: 6),
                    child: Text(
                      'CRASH DETECTED — swipe UP/DOWN twice on the kit to ack',
                      textAlign: TextAlign.center,
                      style: TextStyle(
                        color: Colors.redAccent,
                        fontWeight: FontWeight.bold,
                        fontSize: 12,
                      ),
                    ),
                  ),
              ],
            ),
          ),
          Expanded(
            child: ListView(
              children: [
                _sectionCard('TELEMETRY', [
                  _row('Accel magnitude', _fmt(_accelMagG, unit: ' g')),
                  _row('Pitch', _fmt(_pitchDeg, unit: '°', decimals: 1)),
                  _row('Gyro-X', _fmt(_gyroXDps, unit: ' dps', decimals: 1)),
                  _row('Temp', _fmt(_tempC, unit: '°C', decimals: 1)),
                  _row('Pressure', _fmt(_pressurePa, unit: ' Pa', decimals: 0)),
                  _row('dP/dt', _fmt(_dPdt, unit: ' Pa/s', decimals: 1)),
                  _row('RGB', _r == null ? '—' : '$_r, $_g, $_b'),
                ]),
                if (_crashEventTime != null)
                  _sectionCard('LAST CRASH EVENT', [
                    _row('Time', _crashEventTime!.toLocal().toString()),
                    _row('Peak G', _fmt(_peakG, unit: ' g')),
                    _row('Peak Pressure', _fmt(_peakPressurePa, unit: ' Pa', decimals: 0)),
                  ]),
                _sectionCard('PRE-IMPACT BUFFER (5s @ 100Hz)', [
                  _row('Status', _prebufStreaming
                      ? 'Streaming... (${_preBuffer.length}${_prebufExpectedCount != null ? '/$_prebufExpectedCount' : ''})'
                      : _preBuffer.isEmpty
                          ? 'No data yet'
                          : 'Received ${_preBuffer.length} samples'),
                ]),
                const SizedBox(height: 20),
              ],
            ),
          ),
        ],
      ),
    );
  }
}