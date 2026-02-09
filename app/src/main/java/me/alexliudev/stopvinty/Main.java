package me.alexliudev.stopvinty;

import de.robv.android.xposed.*;
import de.robv.android.xposed.callbacks.XC_LoadPackage;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.regex.*;

public class Main implements IXposedHookLoadPackage {

    private static final String[][] REPLACEMENT_MAP = {
        {"lineageos", "coloros"},
        {"lineage", "color"},
        {"los", "cos"}
    };

    @Override
    public void handleLoadPackage(XC_LoadPackage.LoadPackageParam lpparam) throws Throwable {
        try {
            Class<?> buildClass = XposedHelpers.findClass("android.os.Build", lpparam.classLoader);
            XposedBridge.log(String.format(
                "[FuckLOS] Hooking Build class for package: %s (process: %s)",
                lpparam.packageName, lpparam.processName
            ));

            String[] targetFields = {
                "MANUFACTURER", "MODEL", "BRAND", "DEVICE", "PRODUCT",
                "HARDWARE", "FINGERPRINT", "ID", "DISPLAY"
            };

            int modifiedCount = 0;
            for (String fieldName : targetFields) {
                try {
                    Field field = buildClass.getDeclaredField(fieldName);
                    field.setAccessible(true);

                    Field modifiersField = Field.class.getDeclaredField("modifiers");
                    modifiersField.setAccessible(true);
                    modifiersField.setInt(field, field.getModifiers() & ~Modifier.FINAL);

                    String original = (String) field.get(null);
                    if (original == null || original.isEmpty()) {
                        XposedBridge.log(String.format(
                            "[FuckLOS] %s: Build.%s is null/empty, skipped",
                            lpparam.packageName, fieldName
                        ));
                        continue;
                    }

                    String replaced = applySmartReplacement(original);
                    if (!original.equals(replaced)) {
                        field.set(null, replaced);
                        modifiedCount++;
                        XposedBridge.log(String.format(
                            "[FuckLOS] %s: Replaced Build.%s | '%s' → '%s'",
                            lpparam.packageName, fieldName, original, replaced
                        ));
                    }
                } catch (NoSuchFieldException e) {
                    XposedBridge.log(String.format(
                        "[FuckLOS] %s: Build.%s not found (safe to ignore)",
                        lpparam.packageName, fieldName
                    ));
                } catch (Exception e) {
                    XposedBridge.log(String.format(
                        "[FuckLOS] %s: Failed to modify Build.%s: %s",
                        lpparam.packageName, fieldName, e.getMessage()
                    ));
                }
            }

            XposedBridge.log(String.format(
                "[FuckLOS] %s: Finished (%d fields modified)",
                lpparam.packageName, modifiedCount
            ));

        } catch (Exception e) {
            XposedBridge.log(String.format(
                "[FuckLOS] %s: Critical error during hook: %s",
                lpparam.packageName, e.toString()
            ));
        }
    }

    private static String applySmartReplacement(String input) {
        String result = input;
        for (String[] mapping : REPLACEMENT_MAP) {
            result = replacePreservingCase(result, mapping[0], mapping[1]);
        }
        return result;
    }

    private static String replacePreservingCase(String text, String keyword, String target) {
        Pattern pattern = Pattern.compile(Pattern.quote(keyword), Pattern.CASE_INSENSITIVE);
        Matcher matcher = pattern.matcher(text);
        StringBuffer sb = new StringBuffer();

        while (matcher.find()) {
            String matched = matcher.group(); // such as "LiNeAgE"
            String replacement = adaptCase(target, matched); // → "CoLoR"
            matcher.appendReplacement(sb, Matcher.quoteReplacement(replacement));
        }
        matcher.appendTail(sb);
        return sb.toString();
    }

    private static String adaptCase(String base, String reference) {
        StringBuilder result = new StringBuilder();
        int len = Math.min(base.length(), reference.length());

        for (int i = 0; i < len; i++) {
            char c = base.charAt(i);
            char refChar = reference.charAt(i);
            result.append(Character.isUpperCase(refChar) ? 
                          Character.toUpperCase(c) : 
                          Character.toLowerCase(c));
        }

        if (base.length() > reference.length()) {
            result.append(base.substring(len).toLowerCase());
        }
        return result.toString();
    }
}
