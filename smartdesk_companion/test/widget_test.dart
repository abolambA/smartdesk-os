import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:smartdesk_companion/main.dart';

void main() {
  testWidgets('App launches without error', (WidgetTester tester) async {
    SharedPreferences.setMockInitialValues({});
    final prefs = await SharedPreferences.getInstance();
    await tester.pumpWidget(SmartDeskApp(prefs: prefs));
    expect(find.byType(SmartDeskApp), findsOneWidget);
  });
}
