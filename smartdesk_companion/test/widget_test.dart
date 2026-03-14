import 'package:flutter_test/flutter_test.dart';
import 'package:smartdesk_companion/main.dart';

void main() {
  testWidgets('SmartDesk app smoke test', (WidgetTester tester) async {
    await tester.pumpWidget(const SmartDeskApp());
    expect(find.byType(SmartDeskApp), findsOneWidget);
  });
}
