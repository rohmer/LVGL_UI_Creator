import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.Locale;
import java.util.regex.Pattern;

import opennlp.tools.postag.POSModel;
import opennlp.tools.postag.POSTaggerME;
import opennlp.tools.sentdetect.SentenceDetectorME;
import opennlp.tools.sentdetect.SentenceModel;
import opennlp.tools.tokenize.TokenizerME;
import opennlp.tools.tokenize.TokenizerModel;

class SentenceWorker {
    private static final Pattern END = Pattern.compile("(?<!\\.)[.!?][\\\"'’”»)}\\]]*$");
    private static final Pattern GREEK_END = Pattern.compile("(?<!\\.)[.!?;\u037e][\\\"'’”»)}\\]]*$");
    private final SentenceDetectorME detector;
    private final TokenizerME tokenizer;
    private final POSTaggerME tagger;
    private final boolean english;
    private final boolean greek;
    private final Pattern ending;

    private SentenceWorker(String language) throws IOException {
        String treebank = switch (language) {
            case "en" -> "ewt";
            case "el" -> "gdt";
            case "es", "fr", "pt", "de" -> "gsd";
            default -> throw new IOException("Unsupported OpenNLP language: " + language);
        };
        english = language.equals("en");
        greek = language.equals("el");
        ending = greek ? GREEK_END : END;
        String prefix = "opennlp-" + language + "-ud-" + treebank + "-";
        try (var sentences = model(prefix, "sentence"); var tokens = model(prefix, "tokens"); var pos = model(prefix, "pos")) {
            detector = new SentenceDetectorME(new SentenceModel(sentences));
            tokenizer = new TokenizerME(new TokenizerModel(tokens));
            tagger = new POSTaggerME(new POSModel(pos));
        }
    }

    private static InputStream model(String prefix, String task) throws IOException {
        String name = prefix + task + "-1.3-2.5.4.bin";
        var stream = SentenceWorker.class.getClassLoader().getResourceAsStream(name);
        if (stream == null) throw new IOException("Missing OpenNLP model: " + name);
        return stream;
    }

    public static void main(String[] args) throws IOException {
        if (args.length != 1) throw new IOException("Pass one OpenNLP language code.");
        var worker = new SentenceWorker(args[0]);
        var decoder = Base64.getDecoder();
        var encoder = Base64.getEncoder();
        var input = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8));
        System.out.println("READY");
        for (String line; (line = input.readLine()) != null; ) {
            String text = new String(decoder.decode(line), StandardCharsets.UTF_8);
            String result = worker.sentences(text);
            System.out.println(encoder.encodeToString(result.getBytes(StandardCharsets.UTF_8)));
        }
    }

    private String sentences(String text) {
        var result = new StringBuilder();
        for (String block : text.split("\n")) {
            // The Greek model scans .!? by default. Map its two question marks
            // to '?' for detection, then take the same offsets from the original.
            String boundaries = greek ? block.replace(';', '?').replace('\u037e', '?') : block;
            for (var span : detector.sentPosDetect(boundaries)) {
                String sentence = block.substring(span.getStart(), span.getEnd()).strip();
                if (!isComplete(sentence)) continue;
                if (result.length() > 0) result.append('\n');
                result.append(sentence);
            }
        }
        return result.toString();
    }

    private boolean isComplete(String sentence) {
        if (!ending.matcher(sentence).find()) return false;

        String[] words = tokenizer.tokenize(sentence);
        if (words.length == 0) return false;
        String[] tags = tagger.tag(words);
        // Every bundled model uses Universal Dependencies tags.
        if (!english) {
            for (String tag : tags)
                if (tag.equals("VERB") || tag.equals("AUX")) return true;
            return false;
        }

        int start = 0;
        while (start < tags.length && tags[start].equals("PUNCT")) start++;
        if (start == tags.length) return false;

        // An introductory dependent clause needs a main clause after its comma.
        if (tags[start].equals("SCONJ") && sentence.indexOf('?') < 0) {
            while (start < words.length && !words[start].equals(",")) start++;
            start++;
        }

        for (int i = start; i < tags.length; i++) {
            String word = words[i].toLowerCase(Locale.ROOT);
            if (tags[i].equals("AUX") && !word.equals("being") && !word.equals("having")) return true;
            if (tags[i].equals("VERB") && !word.endsWith("ing") &&
                (i == 0 || !words[i - 1].equalsIgnoreCase("to"))) return true;
        }
        return false;
    }
}
