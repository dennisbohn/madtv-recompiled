// MadTV-Launcher: startet das DOS-Spiel MadTV mit einem eingebetteten DOSBox,
// ohne dass der Nutzer DOSBox selbst bedienen muss.
//
// Ablauf:
//   1. Eingebettete DOSBox-Dateien nach %LOCALAPPDATA%\MadTV\runtime entpacken
//      (nur wenn sie fehlen oder sich geändert haben).
//   2. Eine DOSBox-Konfiguration erzeugen, die den Ordner "Gamefiles" neben
//      dieser EXE als Laufwerk C: einbindet und MTV.EXE startet.
//   3. DOSBox ohne Konsolenfenster starten und nach Spielende beenden.
//
// Optionen:  MadTV.exe -fullscreen   (Vollbild; im Spiel auch Alt+Enter)
//            MadTV.exe -window       (erzwingt Fenstermodus)

using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text;
using System.Windows.Forms;

[assembly: AssemblyTitle("MadTV")]
[assembly: AssemblyProduct("MadTV")]
[assembly: AssemblyVersion("1.0.0.0")]

static class MadTVLauncher
{
    static readonly string[] RuntimeFiles = { "DOSBox.exe", "SDL.dll", "SDL_net.dll" };

    [STAThread]
    static int Main(string[] args)
    {
        try
        {
            return Run(args);
        }
        catch (Exception ex)
        {
            Error("Unerwarteter Fehler:\n\n" + ex.Message);
            return 1;
        }
    }

    static int Run(string[] args)
    {
        string exeDir = AppDomain.CurrentDomain.BaseDirectory;
        string gameDir = FindGameDir(exeDir);
        if (gameDir == null)
        {
            Error("Die Spieldateien wurden nicht gefunden.\n\n" +
                  "Lege den Ordner \"Gamefiles\" (mit MTV.EXE) neben MadTV.exe.\n\n" +
                  "Gesucht in:\n" + exeDir);
            return 1;
        }

        string dataDir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "MadTV");
        string runtimeDir = Path.Combine(dataDir, "runtime");
        Directory.CreateDirectory(runtimeDir);
        ExtractRuntime(runtimeDir);

        bool fullscreen = false;
        string userConf = Path.Combine(exeDir, "madtv-settings.conf");
        foreach (string a in args)
        {
            string arg = a.TrimStart('-', '/').ToLowerInvariant();
            if (arg == "fullscreen" || arg == "f") fullscreen = true;
            if (arg == "window" || arg == "w") fullscreen = false;
        }

        string confPath = Path.Combine(dataDir, "madtv.conf");
        File.WriteAllText(confPath, BuildConfig(gameDir, dataDir, fullscreen), new UTF8Encoding(false));

        var psi = new ProcessStartInfo
        {
            FileName = Path.Combine(runtimeDir, "DOSBox.exe"),
            WorkingDirectory = runtimeDir,
            UseShellExecute = false,
        };
        // Optionale Nutzer-Konfiguration neben der EXE überschreibt die Standardwerte.
        psi.Arguments = "-noconsole -conf \"" + confPath + "\"" +
                        (File.Exists(userConf) ? " -conf \"" + userConf + "\"" : "");

        using (Process p = Process.Start(psi))
        {
            p.WaitForExit();
            return p.ExitCode;
        }
    }

    static string FindGameDir(string exeDir)
    {
        string[] candidates = { Path.Combine(exeDir, "Gamefiles"), exeDir };
        foreach (string dir in candidates)
            if (File.Exists(Path.Combine(dir, "MTV.EXE")))
                return Path.GetFullPath(dir).TrimEnd('\\');
        return null;
    }

    static void ExtractRuntime(string runtimeDir)
    {
        Assembly asm = Assembly.GetExecutingAssembly();
        foreach (string name in RuntimeFiles)
        {
            using (Stream res = asm.GetManifestResourceStream(name))
            {
                if (res == null)
                    throw new InvalidOperationException("Eingebettete Datei fehlt: " + name);

                string target = Path.Combine(runtimeDir, name);
                if (File.Exists(target) && new FileInfo(target).Length == res.Length)
                    continue;

                using (FileStream fs = File.Create(target))
                    res.CopyTo(fs);
            }
        }
    }

    static string BuildConfig(string gameDir, string dataDir, bool fullscreen)
    {
        var sb = new StringBuilder();
        sb.AppendLine("[sdl]");
        sb.AppendLine("fullscreen=" + (fullscreen ? "true" : "false"));
        sb.AppendLine("fullresolution=desktop");
        sb.AppendLine("windowresolution=original");
        sb.AppendLine("output=opengl");
        sb.AppendLine("autolock=false");
        sb.AppendLine("mapperfile=" + Path.Combine(dataDir, "mapper.map"));
        sb.AppendLine();
        sb.AppendLine("[dosbox]");
        sb.AppendLine("machine=svga_s3");
        sb.AppendLine("memsize=16");
        sb.AppendLine("captures=" + Path.Combine(dataDir, "capture"));
        sb.AppendLine();
        sb.AppendLine("[render]");
        sb.AppendLine("aspect=true");
        sb.AppendLine("scaler=normal3x");
        sb.AppendLine();
        sb.AppendLine("[cpu]");
        sb.AppendLine("core=auto");
        sb.AppendLine("cputype=auto");
        sb.AppendLine("cycles=auto");
        sb.AppendLine();
        sb.AppendLine("[mixer]");
        sb.AppendLine("rate=44100");
        sb.AppendLine();
        sb.AppendLine("[sblaster]");
        sb.AppendLine("sbtype=sb16");
        sb.AppendLine("oplrate=44100");
        sb.AppendLine();
        sb.AppendLine("[dos]");
        sb.AppendLine("keyboardlayout=auto");
        sb.AppendLine();
        sb.AppendLine("[autoexec]");
        sb.AppendLine("@echo off");
        sb.AppendLine("mount c \"" + gameDir + "\"");
        sb.AppendLine("c:");
        sb.AppendLine("cls");
        sb.AppendLine("MTV.EXE");
        sb.AppendLine("exit");
        return sb.ToString();
    }

    static void Error(string msg)
    {
        MessageBox.Show(msg, "MadTV", MessageBoxButtons.OK, MessageBoxIcon.Error);
    }
}
