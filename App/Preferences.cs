// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
using System.Configuration;
namespace FrameBridge
{
internal sealed class Preferences : ApplicationSettingsBase
{
    [UserScopedSetting, DefaultSettingValue("")]
    public string Model
    {
        get {
            return (string)this["Model"];
        }
        set {
            this["Model"] = value;
        }
    }
    [UserScopedSetting, DefaultSettingValue("Midnight")]
    public string Theme
    {
        get {
            return (string)this["Theme"];
        }
        set {
            this["Theme"] = value;
        }
    }
    [UserScopedSetting, DefaultSettingValue("False")]
    public bool Maximize
    {
        get {
            return (bool)this["Maximize"];
        }
        set {
            this["Maximize"] = value;
        }
    }
    [UserScopedSetting, DefaultSettingValue("False")]
    public bool Logging
    {
        get {
            return (bool)this["Logging"];
        }
        set {
            this["Logging"] = value;
        }
    }
    [UserScopedSetting, DefaultSettingValue("1")]
    public int Quality
    {
        get {
            return (int)this["Quality"];
        }
        set {
            this["Quality"] = value;
        }
    }
    [UserScopedSetting, DefaultSettingValue("True")]
    public bool Artifact
    {
        get {
            return (bool)this["Artifact"];
        }
        set {
            this["Artifact"] = value;
        }
    }
    [UserScopedSetting, DefaultSettingValue("True")]
    public bool Generate
    {
        get {
            return (bool)this["Generate"];
        }
        set {
            this["Generate"] = value;
        }
    }
    [UserScopedSetting, DefaultSettingValue("1")]
    public int Buffer
    {
        get {
            return (int)this["Buffer"];
        }
        set {
            this["Buffer"] = value;
        }
    }
}
}
