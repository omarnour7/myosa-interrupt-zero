class LowPassFilter {
  final double alpha;
  double? _filtered;

  LowPassFilter({required this.alpha});

  double filter(double input) {
    if (_filtered == null) {
      _filtered = input;
    } else {
      _filtered = alpha * _filtered! + (1.0 - alpha) * input;
    }

    return _filtered!;
  }

  void reset() {
    _filtered = null;
  }

  double? get value => _filtered;
}


class MovingAverageFilter {
  final int windowSize;
  final List<double> _buffer = [];

  MovingAverageFilter({this.windowSize = 5});

  double filter(double input) {
    _buffer.add(input);

    if (_buffer.length > windowSize) {
      _buffer.removeAt(0);
    }

    double sum = 0.0;

    for (final value in _buffer) {
      sum += value;
    }

    return sum / _buffer.length;
  }

  void reset() {
    _buffer.clear();
  }

  double? get value {
    if (_buffer.isEmpty) return null;

    double sum = 0.0;

    for (final value in _buffer) {
      sum += value;
    }

    return sum / _buffer.length;
  }
}