package me.alexliudev.stopvinty;

import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;
import de.robv.android.xposed.callbacks.XC_LoadPackage;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Main implements IXposedHookLoadPackage {

    private static final String[][] BASE_MAPPINGS = {
        {"lineageos", "coloros"},
        {"los", "cos"},
        {"lineage", "color"}
    };

    @Override
    public void handleLoadPackage(XC_LoadPackage.LoadPackageParam lpparam) throws Throwable {
        if (!lpparam.packageName.equals("android")) return;

        Class<?> buildClass = XposedHelpers.findClass("android.os.Build", lpparam.classLoader);

        String[] fieldsToCheck = {
            "MANUFACTURER", "MODEL", "BRAND", "DEVICE", "PRODUCT",
            "HARDWARE", "FINGERPRINT", "ID", "DISPLAY"
        };

        for (String fieldName : fieldsToCheck) {
            try {
                Field field = buildClass.getDeclaredField(fieldName);
                field.setAccessible(true);

                Field modifiersField = Field.class.getDeclaredField("modifiers");
                modifiersField.setAccessible(true);
                modifiersField.setInt(field, field.getModifiers() & ~Modifier.FINAL);

                String originalValue = (String) field.get(null);
                if (originalValue == null) continue;

                String newValue = applySmartReplacements(originalValue);
                if (!originalValue.equals(newValue)) {
                    field.set(null, newValue);
                    XposedBridge.log("StopVinty: Replaced Build." + fieldName + " from '" + originalValue + "' to '" + newValue + "'");
                }
            } catch (Exception e) {
                XposedBridge.log("StopVinty: Failed to hook Build." + fieldName + ": " + e.getMessage());
            }
        }
    }

    private static String applySmartReplacements(String input) {
        String result = input;
 
        for (String[] mapping : new String[][]{
            {"lineageos", "coloros"},
            {"lineage", "color"},
            {"los", "cos"}
        }) {
            String keyword = mapping[0];
            String targetBase = mapping[1];
            result = replaceWithCasePreserved(result, keyword, targetBase);
        }
        return result;
    }

    private static String replaceWithCasePreserved(String text, String keyword, String targetBase) {
        Pattern pattern = Pattern.compile(Pattern.quote(keyword), Pattern.CASE_INSENSITIVE);
        Matcher matcher = pattern.matcher(text);
        StringBuffer sb = new StringBuffer();

        while (matcher.find()) {
            String matched = matcher.group(); // 如 "LineageOS"
            String replacement = adaptCase(targetBase, matched);
            matcher.appendReplacement(sb, Matcher.quoteReplacement(replacement));
        }
        matcher.appendTail(sb);
        return sb.toString();
    }

    private static String adaptCase(String baseStr, String matchStr) {
        StringBuilder result = new StringBuilder();
        int len = Math.min(baseStr.length(), matchStr.length());

        for (int i = 0; i < len; i++) {
            char c = baseStr.charAt(i);
            char ref = matchStr.charAt(i);
            if (Character.isUpperCase(ref)) {
                result.append(Character.toUpperCase(c));
            } else {
                result.append(Character.toLowerCase(c));
            }
        }

        if (baseStr.length() > matchStr.length()) {
            result.append(baseStr.substring(len));
        }

        return result.toString();
    }
}
