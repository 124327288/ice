// **********************************************************************
//
// Copyright (c) 2009 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// LICENSE file included in this distribution.
//
// **********************************************************************

using System;
using System.Text;
using System.Collections.Generic;
using System.ComponentModel;
using EnvDTE;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.IO;
using System.Diagnostics;
using Extensibility;
using EnvDTE80;
using Microsoft.VisualStudio.CommandBars;
using Microsoft.VisualStudio.VCProjectEngine;
using Microsoft.VisualStudio.VCProject;
using Microsoft.VisualStudio.Shell;
using System.Resources;
using System.Reflection;
using VSLangProj;
using System.Globalization;

using System.Collections;
using System.Runtime.InteropServices.ComTypes;
using Microsoft.CSharp;

namespace Ice.VisualStudio
{
    public class IceComponentConverter : TypeConverter
    {
        public override bool CanConvertFrom(ITypeDescriptorContext context, Type t)
        {
            return true;
        }

        public override object ConvertFrom(ITypeDescriptorContext context, CultureInfo info,
                                           object value)
        {
            if(value is System.String)
            {
                string s = (string)value;
                return new IncludePathList(s);
            }
            return base.ConvertFrom(context, info, value);
        }

        public override object ConvertTo(ITypeDescriptorContext context, CultureInfo culture,
                                         object value, Type destType)
        {
            if(destType == typeof(System.String) && value is IncludePathList)
            {
                IncludePathList paths = (IncludePathList)value;
                StringBuilder sb = new StringBuilder();
                for(int cont = 0; cont < paths.Count; cont++)
                {
                    sb.Append(paths[cont]);
                    if(cont < paths.Count - 1)
                    {
                        sb.Append("; ");
                    }
                }
                return sb.ToString();
            }
            return base.ConvertTo(context, culture, value, destType);
        }
    }

    public class IncludePathConverter : TypeConverter
    {
        public override bool CanConvertFrom(ITypeDescriptorContext context, Type t)
        {
            return true;
        }

        public override object ConvertFrom(ITypeDescriptorContext context, CultureInfo info,
                                           object value)
        {
            if(value is System.String)
            {
                string s = (string)value;
                return new IncludePathList(s);
            }
            return base.ConvertFrom(context, info, value);
        }

        public override object ConvertTo(ITypeDescriptorContext context, CultureInfo culture,
                                         object value, Type destType)
        {
            if(destType == typeof(System.String) && value is IncludePathList)
            {
                IncludePathList paths = (IncludePathList)value;
                StringBuilder sb = new StringBuilder();
                for(int cont = 0; cont < paths.Count; cont++)
                {
                    sb.Append(paths[cont]);
                    if(cont < paths.Count - 1)
                    {
                        sb.Append("; ");
                    }
                }
                return sb.ToString();
            }
            return base.ConvertTo(context, culture, value, destType);
        }

    }

    [TypeConverter(typeof(Ice.VisualStudio.IceComponentConverter))]
    public class ComponentList : List<string>
    {
        public ComponentList()
        {
        }

        public ComponentList(string[] values)
        {
            foreach(string s in values)
            {
                Add(s);
            }
        }

        public new void Add(string value)
        {
            value = value.Trim();
            if(!base.Contains(value))
            {
                base.Add(value);
            }
        }

        public new bool Contains(string value)
        {
            string found = base.Find(delegate(string s)
                                    {
                                        return s.Equals(value, StringComparison.CurrentCultureIgnoreCase);
                                    });
            return !String.IsNullOrEmpty(found);
        }

        public new void Remove(string value)
        {
            string found = base.Find(delegate(string s)
            {
                return s.Equals(value, StringComparison.CurrentCultureIgnoreCase);
            });

            if(!String.IsNullOrEmpty(found))
            {
                base.Remove(found);
            }
        }

        public ComponentList(string value)
        {
            init(value, ';');
        }

        public ComponentList(string value, char separator)
        {
            init(value, separator);
        }

        private void init(string value, char separator)
        {
            Array items = value.Split(separator);
            foreach(string s in items)
            {
                s.Trim();
                if(s.Length > 0)
                {
                    Add(s.Trim());
                }
            }
        }

        public override string ToString()
        {
            return ToString(';');
        }

        public string ToString(char separator)
        {
            StringBuilder sb = new StringBuilder();
            for(int cont = 0; cont < this.Count; cont++)
            {
                sb.Append(this[cont]);
                if(cont < this.Count - 1)
                {
                    sb.Append(separator);
                    if(!separator.Equals(' '))
                    {
                        sb.Append(' ');
                    }
                }
            }
            return sb.ToString();
        }
    }

    public class IncludePathList : ComponentList
    {
        public IncludePathList()
        {
        }

        public IncludePathList(string[] values)
            : base(values)
        {
        }

        public IncludePathList(string value)
            : base(value)
        {
        }
    }

    public class Util
    {
        public class SliceTranslator
        {
            public const string slice2cs = "slice2cs.exe";
            public const string slice2cpp = "slice2cpp.exe";
            public const string slice2sl = "slice2sl.exe";
        };

        //
        // Property names used to persist project configuration.
        //
        public class PropertyNames
        {
            public const string Ice = "ZerocIce_Enabled";
            public const string IceHome = "ZerocIce_Home";
            public const string ComponentList = "ZerocIce_ComponentList";
            public const string IceExtraOptions = "ZerocIce_ExtraOptions";
            public const string IceIncludePath = "ZerocIce_IncludePath";
            public const string IceStreaming = "ZerocIce_Streaming";
            public const string IceTie = "ZerocIce_Tie";
            public const string IcePrefix = "ZerocIce_Prefix";
            public const string IceDllExport = "ZerocIce_DllExport";
            public const string ConsoleOutput = "ZerocIce_ConsoleOutput";
        }

        public class ComponentNames
        {
            public static readonly string[] silverlightNames =
            {
                "IceSL"
            };
            
            public static readonly string[] cppNames =
            {
                "Freeze", "Glacier2", "Ice", "IceBox", "IceGrid", "IcePatch2", 
                "IceSSL", "IceStorm", "IceUtil" 
            };

            public static readonly string[] cSharpNames =
            {
                "Glacier2", "Ice", "IceBox", "IceGrid", "IcePatch2", 
                "IceSSL", "IceStorm", "IceUtil"
            };
        }

        public static string getIceHome(Project project)
        {
            const string iceSilverlightHome = "C:\\IceSL-0.3.3";
#if VS2008
            const string defaultIceHome = "C:\\Ice-3.3.1-VC90";
#else
            const string defaultIceHome = "C:\\Ice-3.3.1";
#endif
            if(Util.isSilverlightProject(project))
            {
                return Util.getProjectProperty(project, Util.PropertyNames.IceHome, iceSilverlightHome);
            }
            return Util.getProjectProperty(project, Util.PropertyNames.IceHome, defaultIceHome);
        }
        
        public static string getAbsoluteIceHome(Project project)
        {
            string iceHome = Util.getIceHome(project);
            if(!Path.IsPathRooted(iceHome))
            {
                iceHome = Path.Combine(Path.GetDirectoryName(project.FileName), iceHome);
                iceHome = Path.GetFullPath(iceHome);
            }
            return iceHome;
        }

        public static string getPathRelativeToProject(ProjectItem item)
        {
            StringBuilder path = new StringBuilder();
            if(item != null)
            {
                path.Append(Util.getPathRelativeToProject(item, item.ContainingProject.ProjectItems));
            }
            return Util.normalizePath(path.ToString());
        }

        public static string getPathRelativeToProject(ProjectItem item, ProjectItems items)
        {
            StringBuilder path = new StringBuilder();
            foreach(ProjectItem i in items)
            {
                if(i == item)
                {
                    if(path.Length > 0)
                    {
                        path.Append("\\");
                    }
                    path.Append(i.Name);
                    break;
                }
                else if(Util.isProjectItemFilter(i) || Util.isProjectItemFolder(i))
                {
                    string token = Util.getPathRelativeToProject(item, i.ProjectItems);
                    if(!String.IsNullOrEmpty(token))
                    {
                        path.Append(i.Name);
                        path.Append("\\");
                        path.Append(token);
                        break;
                    }
                }
            }
            return path.ToString();
        }

        public static void addCppIncludes(VCCLCompilerTool tool, Project project)
        {
            if(tool == null || project == null)
            {
                return;
            }

            string iceHome = Util.getAbsoluteIceHome(project);
            string iceIncludeDir = "";
            if(Directory.Exists(iceHome + "\\cpp\\include"))
            {
                iceIncludeDir = Util.getIceHome(project) + "\\cpp\\include";
            }
            else
            {
                iceIncludeDir = Util.getIceHome(project) + "\\include";
            }

            string additionalIncludeDirectories = tool.AdditionalIncludeDirectories;
            if(String.IsNullOrEmpty(additionalIncludeDirectories))
            {
                tool.AdditionalIncludeDirectories = iceIncludeDir + ";.";
                return;
            }

            ComponentList includes = new ComponentList(additionalIncludeDirectories);
            bool changed = false;
            if(String.IsNullOrEmpty(includes.Find(delegate(string d)
                                                    {
                                                        return d.Equals(iceIncludeDir,
                                                                        StringComparison.CurrentCultureIgnoreCase);
                                                    })))
            {
                changed = true;
                includes.Add(iceIncludeDir);
            }

            if(String.IsNullOrEmpty(includes.Find(delegate(string d)
                                                    {
                                                        return d.Equals(".",
                                                                        StringComparison.CurrentCultureIgnoreCase);
                                                    })))
            {
                changed = true;
                includes.Add(".");
            }

            if(changed)
            {
                tool.AdditionalIncludeDirectories = includes.ToString();
            }
        }

        public static void removeCppIncludes(VCCLCompilerTool tool, Project project)
        {
            if(tool == null || project == null)
            {
                return;
            }

            string iceHome = Util.getAbsoluteIceHome(project);
            string iceIncludeDir = "";
            if(Directory.Exists(iceHome + "\\cpp\\include"))
            {
                iceIncludeDir = Util.getIceHome(project) + "\\cpp\\include";
            }
            else
            {
                iceIncludeDir = Util.getIceHome(project) + "\\include";
            }
            
            string additionalIncludeDirectories = tool.AdditionalIncludeDirectories;
            if(String.IsNullOrEmpty(additionalIncludeDirectories))
            {
                return;
            }

            ComponentList includes = new ComponentList(additionalIncludeDirectories);
            string includeDir = includes.Find(delegate(string d)
                                                {
                                                    return d.Equals(iceIncludeDir, 
                                                                    StringComparison.CurrentCultureIgnoreCase);
                                                });

            if(!String.IsNullOrEmpty(includeDir))
            {
                includes.Remove(includeDir);
                tool.AdditionalIncludeDirectories = includes.ToString();
            }
        }

        public static void addCSharpReference(Project project, string component)
        {
            if(project == null || String.IsNullOrEmpty(component))
            {
                return;
            }

            string iceHome = Util.getAbsoluteIceHome(project);
            string reference = "";
            if(Directory.Exists(iceHome + "\\cs"))
            {
                reference = iceHome + "\\cs\\bin\\" + component + ".dll";                
            }
            else if(Directory.Exists(iceHome + "\\sl"))
            {
                reference = iceHome + "\\sl\\bin\\" + component + ".dll";
            }
            else
            {
                reference = iceHome + "\\bin\\" + component + ".dll";
            }

            if(File.Exists(reference))
            {
                VSLangProj.VSProject vsProject = (VSLangProj.VSProject)project.Object;
                try
                {
                    vsProject.References.Add(reference);
                }
                catch(COMException ex)
                {
                    Console.WriteLine(ex);
                }
            }
            else
            {
                System.Windows.Forms.MessageBox.Show("Could not locate '" + component + 
                                                     ".dll'. Review you 'Ice Home' setting.",
                                                     "Ice Visual Studio Extension", MessageBoxButtons.OK,
                                                     MessageBoxIcon.Error);
                return;
            }
        }

        public static Reference findReference(VSProject project, string path)
        {
            Reference value = null;
            foreach(Reference r in project.References)
            {
                if(r.Path.Equals(path, StringComparison.CurrentCultureIgnoreCase))
                {
                    value = r;
                    break;
                }
            }
            return value;
        }

        public static void removeCSharpReference(Project project, string component)
        {
            if(project == null || String.IsNullOrEmpty(component))
            {
                return;
            }

            string iceHome = Util.getAbsoluteIceHome(project);
            if(Directory.Exists(iceHome + "\\cs\\bin"))
            {
                iceHome += "\\cs\\bin";
            }
            else if(Directory.Exists(iceHome + "\\sl\\bin"))
            {
                iceHome += "\\sl\\bin";
            }
            else
            {
                iceHome += "\\bin";
            }
            Reference reference = Util.findReference((VSProject)project.Object,
                                                     Path.Combine(iceHome, component + ".dll"));
            if(reference != null)
            {
                reference.Remove();
            }
        }

        public static void addCppLib(VCLinkerTool tool, string component, bool debug)
        {
            if(tool == null || String.IsNullOrEmpty(component))
            {
                return;
            }

            string iceComponent = Array.Find(Util.ComponentNames.cppNames, delegate(string name)
                                                                            {
                                                                                return name.Equals(component);
                                                                            });

            if(String.IsNullOrEmpty(iceComponent))
            {
                return;
            }

            string libName = component;
            if(debug)
            {
                libName += "d";
            }
            libName += ".lib";

            string additionalDependencies = tool.AdditionalDependencies;
            if(String.IsNullOrEmpty(additionalDependencies))
            {
                additionalDependencies = "";
            }

            ComponentList components = new ComponentList(additionalDependencies.Split(' '));
            if(!components.Contains(libName))
            {
                components.Add(libName);
                additionalDependencies = components.ToString(' ');
                tool.AdditionalDependencies = additionalDependencies;
            }
        }

        public static void removeCppLib(VCLinkerTool tool, string component, bool debug)
        {
            if(tool == null)
            {
                return;
            }

            if(String.IsNullOrEmpty(tool.AdditionalDependencies))
            {
                return;
            }

            string iceComponent = Array.Find(Util.ComponentNames.cppNames,
                                            delegate(string v)
                                                {
                                                    return v.Equals(component, StringComparison.OrdinalIgnoreCase);
                                                });

            if(String.IsNullOrEmpty(iceComponent))
            {
                //Not an Ice Lib.
                return;
            }

            string libName = component;
            if(debug)
            {
                libName += "d";
            }
            libName += ".lib";

            ComponentList components = new ComponentList(tool.AdditionalDependencies.Split(' '));
            if(components.Contains(libName))
            {
                components.Remove(libName);
                tool.AdditionalDependencies = components.ToString(' ');
            }
        }

        public static void addIceCppLibraryDir(VCLinkerTool tool, Project project)
        {
            if(tool == null || project == null)
            {
                return;
            }

            string iceHome = Util.getAbsoluteIceHome(project);
            string iceLibDir = "";
            if(Directory.Exists(iceHome + "\\cpp\\lib"))
            {
                iceLibDir = Util.getIceHome(project) + "\\cpp\\lib";
            }
            else
            {
                iceLibDir = Util.getIceHome(project) + "\\lib";
            }
            string additionalLibraryDirectories = tool.AdditionalLibraryDirectories;
            if(String.IsNullOrEmpty(additionalLibraryDirectories))
            {
                tool.AdditionalLibraryDirectories = iceLibDir;
                return;
            }

            ComponentList libs = new ComponentList(additionalLibraryDirectories);
            if(String.IsNullOrEmpty(libs.Find(delegate(string d)
                                                {
                                                    return d.Equals(iceLibDir, 
                                                                    StringComparison.CurrentCultureIgnoreCase);
                                                })))
            {
                libs.Add(iceLibDir);
                tool.AdditionalLibraryDirectories = libs.ToString();
                return;
            }
        }

        public static void removeIceCppLibraryDir(VCLinkerTool tool, Project project)
        {
            if(tool == null || project == null)
            {
                return;
            }

            string iceHome = Util.getAbsoluteIceHome(project);
            string iceLibDir = "";
            if(Directory.Exists(iceHome + "\\cpp\\lib"))
            {
                iceLibDir = Util.getIceHome(project) + "\\cpp\\lib";
            }
            else
            {
                iceLibDir = Util.getIceHome(project) + "\\lib";
            }
            string additionalLibraryDirectories = tool.AdditionalLibraryDirectories;

            if(String.IsNullOrEmpty(additionalLibraryDirectories))
            {
                return;
            }

            ComponentList libs = new ComponentList(additionalLibraryDirectories);

            string libDir = libs.Find(delegate(string d)
                                        {
                                            return d.Equals(iceLibDir, StringComparison.CurrentCultureIgnoreCase);
                                        });

            if(!String.IsNullOrEmpty(libDir))
            {
                libs.Remove(libDir);
                tool.AdditionalLibraryDirectories = libs.ToString();
                return;
            }
        }

        public static bool isSliceBuilderEnabled(Project project)
        {
            return Util.getProjectPropertyAsBool(project, Util.PropertyNames.Ice);
        }

        public static bool isCSharpProject(Project project)
        {
            if(project == null)
            {
                return false;
            }

            if(String.IsNullOrEmpty(project.Kind))
            {
                return false;
            }

            return project.Kind == VSLangProj.PrjKind.prjKindCSharpProject;
        }

        public static bool isSilverlightProject(Project project)
        {
            if(!Util.isCSharpProject(project))
            {
                return false;
            }

            Array extenders = (Array)project.ExtenderNames;
            foreach(string s in extenders)
            {
                if(String.IsNullOrEmpty(s))
                {
                    continue;
                }
                if(s.Equals("SilverlightProject"))
                {
                    return true;
                }
            }
            return false;
        }

        public static bool isCppProject(Project project)
        {
            if(project == null)
            {
                return false;
            }

            if(String.IsNullOrEmpty(project.Kind))
            {
                return false;
            }
            return project.Kind == vcContextGuids.vcContextGuidVCProject;
        }

        public static bool isProjectItemFolder(ProjectItem item)
        {
            if(item == null)
            {
                return false;
            }

            if(String.IsNullOrEmpty(item.Kind))
            {
                return false;
            }
            return item.Kind == "{6BB5F8EF-4483-11D3-8BCF-00C04F8EC28C}";
        }

        public static bool isProjectItemFilter(ProjectItem item)
        {
            if(item == null)
            {
                return false;
            }

            if(String.IsNullOrEmpty(item.Kind))
            {
                return false;
            }
            return item.Kind == "{6BB5F8F0-4483-11D3-8BCF-00C04F8EC28C}";
        }

        public static bool isProjectItemFile(ProjectItem item)
        {
            if(item == null)
            {
                return false;
            }

            if(String.IsNullOrEmpty(item.Kind))
            {
                return false;
            }
            return item.Kind == "{6BB5F8EE-4483-11D3-8BCF-00C04F8EC28C}";
        }

        public static bool hasItemNamed(ProjectItems items, string name)
        {
            bool found = false;
            foreach(ProjectItem item in items)
            {
                if(item == null)
                {
                    continue;
                }

                if(item.Name == null)
                {
                    continue;
                }

                if(item.Name.Equals(name, StringComparison.OrdinalIgnoreCase))
                {
                    found = true;
                    break;
                }
            }
            return found;
        }

        public static ProjectItem findItem(string path, ProjectItems items)
        {
            if(String.IsNullOrEmpty(path))
            {
                return null;
            }
            ProjectItem item = null;
            foreach(ProjectItem i in items)
            {
                if(i == null)
                {
                    continue;
                }
                else if(Util.isProjectItemFile(i))
                {
                    string fullPath = i.Properties.Item("FullPath").Value.ToString();
                    if(Path.GetFullPath(fullPath).Equals(
                           Path.GetFullPath(path), StringComparison.CurrentCultureIgnoreCase))
                    {
                        item = i;
                        break;
                    }
                }
                else if(Util.isProjectItemFolder(i))
                {
                    string p = Path.GetDirectoryName(i.Properties.Item("FullPath").Value.ToString());
                    if(p.Equals(Path.GetFullPath(path), StringComparison.CurrentCultureIgnoreCase))
                    {
                        item = i;
                        break;
                    }

                    item = findItem(path, i.ProjectItems);
                    if(item != null)
                    {
                        break;
                    }
                }
                else if(Util.isProjectItemFilter(i))
                {
                    string p = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(i.ContainingProject.FileName),
                                            Util.normalizePath(Util.getPathRelativeToProject(i))));

                    if(p.Equals(Path.GetFullPath(path), StringComparison.CurrentCultureIgnoreCase))
                    {
                        item = i;
                        break;
                    }

                    item = findItem(path, i.ProjectItems);
                    if(item != null)
                    {
                        break;
                    }
                }
            }
            return item;
        }

        public static VCFile findVCFile(IVCCollection files, string name, string fullPath)
        {
            VCFile vcFile = null;
            foreach(VCFile file in files)
            {
                if(file.ItemName == name)
                {
                    if(file.FullPath != fullPath)
                    {
                        file.Remove();
                        break;
                    }
                    vcFile = file;
                    break;
                }
            }
            return vcFile;
        }

        public static string normalizePath(string path)
        {
            path = path.Replace('/', '\\');
            path = path.Replace(".\\", "");
            if(path.IndexOf("\\") == 0)
            {
                path = path.Substring(1, path.Length - 1);
            }
            if(path.EndsWith("\\."))
            {
                path = path.Substring(0, path.Length - "\\.".Length);
            }
            return path;
        }

        public static string relativePath(string mainDirPath, string absoluteFilePath)
        {
            string[] firstPathParts = 
                mainDirPath.Trim(Path.DirectorySeparatorChar).Split(Path.DirectorySeparatorChar);
            string[] secondPathParts =
                absoluteFilePath.Trim(Path.DirectorySeparatorChar).Split(Path.DirectorySeparatorChar);

            int sameCounter = 0;
            for(int i = 0; i < Math.Min(firstPathParts.Length, secondPathParts.Length); i++)
            {
                if(!firstPathParts[i].Equals(secondPathParts[i], StringComparison.CurrentCultureIgnoreCase))
                {
                    break;
                }
                sameCounter++;
            }

            if(sameCounter == 0)
            {
                return absoluteFilePath;
            }

            string newPath = String.Empty;
            for(int i = sameCounter; i < firstPathParts.Length; i++)
            {
                if(i > sameCounter)
                {
                    newPath += Path.DirectorySeparatorChar;
                }
                newPath += "..";
            }

            if(newPath.Length == 0)
            {
                newPath = ".";
            }

            for(int i = sameCounter; i < secondPathParts.Length; i++)
            {
                newPath += Path.DirectorySeparatorChar;
                newPath += secondPathParts[i];
            }
            return newPath;
        }

        public static ProjectItem getSelectedProjectItem(DTE dte)
        {
            UIHierarchyItem uiItem = getSelectedUIHierearchyItem(dte);
            if(uiItem == null)
            {
                return null;
            }
            return uiItem.Object as ProjectItem;
        }

        public static Project getSelectedProject()
        {
            return Util.getSelectedProject(Util.getCurrentDTE());
        }

        public static Project getSelectedProject(DTE dte)
        {
            UIHierarchyItem uiItem = getSelectedUIHierearchyItem(dte);
            if(uiItem == null)
            {
                return null;
            }
            return uiItem.Object as Project;
        }

        public static UIHierarchyItem getSelectedUIHierearchyItem(DTE dte)
        {
            if(dte == null)
            {
                return null;
            }

            UIHierarchy uiHierarchy = 
                (EnvDTE.UIHierarchy)dte.Windows.Item(EnvDTE.Constants.vsWindowKindSolutionExplorer).Object;
            if(uiHierarchy == null)
            {
                return null;
            }

            if(uiHierarchy.SelectedItems == null)
            {
                return null;
            }

            if(((Array)uiHierarchy.SelectedItems).Length <= 0)
            {
                return null;
            }
            return (UIHierarchyItem)((Array)uiHierarchy.SelectedItems).GetValue(0);
        }

        public static void updateIceHome(Project project, string iceHome)
        {
            if(project == null || String.IsNullOrEmpty(iceHome))
            {
                return;
            }

            string oldIceHome = Util.getIceHome(project).ToUpper();
            if(Path.GetFullPath(oldIceHome).Equals(Path.GetFullPath(iceHome),
                                                   StringComparison.CurrentCultureIgnoreCase))
            {
                return;
            }

            if(Util.isCSharpProject(project))
            {
                updateIceHomeCSharpProject(project, iceHome);
            }
            else if(Util.isCppProject(project))
            {
                updateIceHomeCppProject(project, iceHome);
            }
        }

        private static void updateIceHomeCppProject(Project project, string iceHome)
        {
            Util.removeIceCppConfigurations(project);
            Util.setIceHome(project, iceHome);
            Util.addIceCppConfigurations(project);
        }

        private static void updateIceHomeCSharpProject(Project project, string iceHome)
        {
            ComponentList components = Util.getIceCSharpComponents(project);
            foreach(string s in components)
            {
                if(String.IsNullOrEmpty(s))
                {
                    continue;
                }
                Util.removeCSharpReference(project, s);
            }

            Util.setIceHome(project, iceHome);

            foreach(string s in components)
            {
                if(String.IsNullOrEmpty(s))
                {
                    continue;
                }
                Util.addCSharpReference(project, s);
            }
        }

        public static void setIceHome(Project project, string value)
        {
            if(Util.isSilverlightProject(project))
            {
                if(!File.Exists(value + "\\bin\\slice2sl.exe") || !Directory.Exists(value + "\\slice\\Ice"))
                {
                    if(!File.Exists(value + "\\cpp\\bin\\slice2sl.exe") || !Directory.Exists(value + "\\sl\\slice\\Ice"))
                    {
                        System.Windows.Forms.MessageBox.Show("Could not locate Ice for Silverlight installation in '"
                                                             + value + "' directory.\n",
                                                             "Ice Visual Studio Extension", MessageBoxButtons.OK,
                                                             MessageBoxIcon.Error);
                        return;
                    }
                }
            }
            else
            {
                if(!File.Exists(value + "\\bin\\slice2cpp.exe") || !File.Exists(value + "\\bin\\slice2cs.exe") ||
                   !Directory.Exists(value + "\\slice\\Ice"))
                {
                    if(!File.Exists(value + "\\cpp\\bin\\slice2cpp.exe") || 
                       !File.Exists(value + "\\cpp\\bin\\slice2cs.exe") ||
                       !Directory.Exists(value + "\\slice\\Ice"))
                    {
                        System.Windows.Forms.MessageBox.Show("Could not locate Ice installation in '"
                                                         + value + "' directory.\n",
                                                         "Ice Visual Studio Extension", MessageBoxButtons.OK,
                                                         MessageBoxIcon.Error);

                        return;
                    }
                }
            }
            string projectDir = Path.GetFullPath(Path.GetDirectoryName(project.FileName)) + "\\";
            value = Path.GetFullPath(value);
            if(projectDir.StartsWith(value + "\\", StringComparison.CurrentCultureIgnoreCase))
            {
                value = relativePath(projectDir, value);
            }

            setProjectProperty(project, Util.PropertyNames.IceHome, value);
        }

        public static bool getProjectPropertyAsBool(Project project, string name)
        {
            return Util.getProjectProperty(project, name).Equals(
                                        true.ToString(), StringComparison.CurrentCultureIgnoreCase);
        }

        public static string getProjectProperty(Project project, string name)
        {
            return Util.getProjectProperty(project, name, "");
        }

        public static string getProjectProperty(Project project, string name, string defaultValue)
        {
            if(project == null || String.IsNullOrEmpty(name))
            {
                return defaultValue;
            }

            if(project.Globals == null)
            {
                return defaultValue;
            }

            if(!project.Globals.get_VariableExists(name))
            {
                if(String.IsNullOrEmpty(defaultValue))
                {
                    return "";
                }
                project.Globals[name] = defaultValue;
                project.Globals.set_VariablePersists(name, true);
            }
            return project.Globals[name].ToString();
        }

        public static void setProjectProperty(Project project, string name, string value)
        {
            if(project == null || String.IsNullOrEmpty(name))
            {
                return ;
            }

            if(project.Globals == null)
            {
                return;
            }

            project.Globals[name] = value;
            project.Globals.set_VariablePersists(name, true);
        }
        
        public static String getPrecompileHeader(Project project)
        {
            if(!Util.isCppProject(project))
            {
                return "";
            }
            ConfigurationManager configManager = project.ConfigurationManager;
            Configuration activeConfig = (Configuration)configManager.ActiveConfiguration;

            VCProject vcProject = (VCProject)project.Object;
            IVCCollection configurations = (IVCCollection)vcProject.Configurations;
            VCProjectEngine vcProjectEngine = (VCProjectEngine)configurations.VCProjectEngine;
            IVCCollection platforms = (IVCCollection)vcProjectEngine.Platforms;
            VCPlatform win32Platform = (VCPlatform)platforms.Item("Win32");
            String preCompiledHeader = "";
            foreach(VCConfiguration conf in configurations)
            {
                if(conf.Name != (activeConfig.ConfigurationName + "|" + activeConfig.PlatformName))
                {
                    continue;
                }
                VCCLCompilerTool compilerTool = 
                    (VCCLCompilerTool)(((IVCCollection)conf.Tools).Item("VCCLCompilerTool"));
                if(compilerTool == null)
                {
                    break;
                }
                if(compilerTool.UsePrecompiledHeader == pchOption.pchCreateUsingSpecific ||
                   compilerTool.UsePrecompiledHeader == pchOption.pchUseUsingSpecific)
                {
                    preCompiledHeader = compilerTool.PrecompiledHeaderThrough;
                }
            }
            return preCompiledHeader;
        }

        public static ComponentList getIceCppComponents(Project project)
        {
            ComponentList components = new ComponentList();
            ConfigurationManager configManager = project.ConfigurationManager;
            Configuration activeConfig = (Configuration)configManager.ActiveConfiguration;

            VCProject vcProject = (VCProject)project.Object;
            IVCCollection configurations = (IVCCollection)vcProject.Configurations;
            VCProjectEngine vcProjectEngine = (VCProjectEngine)configurations.VCProjectEngine;
            IVCCollection platforms = (IVCCollection)vcProjectEngine.Platforms;
            VCPlatform win32Platform = (VCPlatform)platforms.Item("Win32");

            foreach(VCConfiguration conf in configurations)
            {
                if(conf.Name != (activeConfig.ConfigurationName + "|" + activeConfig.PlatformName))
                {
                    continue;
                }

                VCCLCompilerTool compilerTool = 
                    (VCCLCompilerTool)(((IVCCollection)conf.Tools).Item("VCCLCompilerTool"));
                VCLinkerTool linkerTool = (VCLinkerTool)(((IVCCollection)conf.Tools).Item("VCLinkerTool"));
                if(linkerTool == null || compilerTool == null)
                {
                    break;
                }

                if(String.IsNullOrEmpty(linkerTool.AdditionalDependencies))
                {
                    break;
                }

                bool debug = false;
                if(!String.IsNullOrEmpty(compilerTool.PreprocessorDefinitions))
                {
                    debug = (compilerTool.PreprocessorDefinitions.Contains("DEBUG") &&
                             !compilerTool.PreprocessorDefinitions.Contains("NDEBUG"));
                }

                if(!debug)
                {
                    debug = conf.Name.Contains("Debug");
                }

                List<string> componentNames = new List<string>(linkerTool.AdditionalDependencies.Split(' '));
                foreach(string s in componentNames)
                {
                    if(String.IsNullOrEmpty(s))
                    {
                        continue;
                    }

                    int index = s.LastIndexOf('.');
                    if(index <= 0)
                    {
                        continue;
                    }

                    string libName = s.Substring(0, index);
                    if(debug)
                    {
                        libName = libName.Substring(0, libName.Length - 1);
                    }
                    if(String.IsNullOrEmpty(libName))
                    {
                        continue;
                    }

                    string iceComponent = Array.Find(Util.ComponentNames.cppNames, delegate(string name)
                                                                                    {
                                                                                        return name.Equals(libName);
                                                                                    });
                    if(String.IsNullOrEmpty(iceComponent))
                    {
                        continue;
                    }
                    components.Add(iceComponent.Trim());
                }
            }
            return components;
        }

        public static ComponentList getIceSilverlightComponents(Project project)
        {
            ComponentList components = new ComponentList();
            if(project == null)
            {
                return components;
            }

            string iceHome = Util.getAbsoluteIceHome(project);
            VSLangProj.VSProject vsProject = (VSLangProj.VSProject)project.Object;
            foreach(Reference r in vsProject.References)
            {
                if(r == null)
                {
                    continue;
                }
                if(String.IsNullOrEmpty(r.Path))
                {
                    continue;
                }
                if(!Path.GetDirectoryName(r.Path).StartsWith(iceHome + "\\", 
                                                             StringComparison.CurrentCultureIgnoreCase))
                {
                    continue; //Not in Ice Home
                }

                string iceComponent = Array.Find(Util.ComponentNames.silverlightNames, delegate(string name)
                {
                    return name.Equals(r.Name);
                });

                if(String.IsNullOrEmpty(iceComponent))
                {
                    continue;
                }

                components.Add(r.Name);
            }
            return components;
        }

        public static ComponentList getIceCSharpComponents(Project project)
        {
            ComponentList components = new ComponentList();
            if(project == null)
            {
                return components;
            }

            string iceHome = Util.getAbsoluteIceHome(project);
            VSLangProj.VSProject vsProject = (VSLangProj.VSProject)project.Object;
            foreach(Reference r in vsProject.References)
            {
                if(!Path.GetDirectoryName(r.Path).ToUpper().Contains(iceHome.ToUpper()))
                {
                    continue; //Not in Ice Home
                }

                string iceComponent = Array.Find(Util.ComponentNames.cSharpNames, delegate(string name)
                {
                    return name.Equals(r.Name);
                });

                if(String.IsNullOrEmpty(iceComponent))
                {
                    continue;
                }

                components.Add(r.Name);
            }
            return components;
        }

        public static void addIceCppConfigurations(Project project)
        {
            if(!isCppProject(project))
            {
                return;
            }

            VCProject vcProject = (VCProject)project.Object;
            IVCCollection configurations = (IVCCollection)vcProject.Configurations;
            VCProjectEngine vcProjectEngine = (VCProjectEngine)configurations.VCProjectEngine;
            IVCCollection platforms = (IVCCollection)vcProjectEngine.Platforms;
            VCPlatform win32Platform = (VCPlatform)platforms.Item("Win32");
            foreach(VCConfiguration conf in configurations)
            {
                if(conf != null)
                {
                    VCCLCompilerTool compilerTool =
                        (VCCLCompilerTool)(((IVCCollection)conf.Tools).Item("VCCLCompilerTool"));
                    VCLinkerTool linkerTool = (VCLinkerTool)(((IVCCollection)conf.Tools).Item("VCLinkerTool"));
                    Util.addIceCppLibraryDir(linkerTool, project);
                    Util.addCppIncludes(compilerTool, project);
                }
            }
        }

        public static void removeIceCppConfigurations(Project project)
        {
            if(!isCppProject(project))
            {
                return;
            }

            VCProject vcProject = (VCProject)project.Object;
            IVCCollection configurations = (IVCCollection)vcProject.Configurations;
            VCProjectEngine vcProjectEngine = (VCProjectEngine)configurations.VCProjectEngine;
            IVCCollection platforms = (IVCCollection)vcProjectEngine.Platforms;
            VCPlatform win32Platform = (VCPlatform)platforms.Item("Win32");
            foreach(VCConfiguration conf in configurations)
            {
                if(conf != null)
                {
                    VCCLCompilerTool compilerTool =
                        (VCCLCompilerTool)(((IVCCollection)conf.Tools).Item("VCCLCompilerTool"));
                    VCLinkerTool linkerTool = (VCLinkerTool)(((IVCCollection)conf.Tools).Item("VCLinkerTool"));

                    Util.removeIceCppLibraryDir(linkerTool, project);
                    Util.removeCppIncludes(compilerTool, project);
                }
            }
        }

        public static void addIceCppLibs(Project project, ComponentList components)
        {
            if(!isCppProject(project))
            {
                return;
            }

            VCProject vcProject = (VCProject)project.Object;
            IVCCollection configurations = (IVCCollection)vcProject.Configurations;
            VCProjectEngine vcProjectEngine = (VCProjectEngine)configurations.VCProjectEngine;
            IVCCollection platforms = (IVCCollection)vcProjectEngine.Platforms;
            VCPlatform win32Platform = (VCPlatform)platforms.Item("Win32");
            foreach(VCConfiguration conf in configurations)
            {
                if(conf != null)
                {
                    VCCLCompilerTool compilerTool = 
                        (VCCLCompilerTool)(((IVCCollection)conf.Tools).Item("VCCLCompilerTool"));
                    VCLinkerTool linkerTool = (VCLinkerTool)(((IVCCollection)conf.Tools).Item("VCLinkerTool"));

                    bool debug = false;
                    if(!String.IsNullOrEmpty(compilerTool.PreprocessorDefinitions))
                    {
                        debug = (compilerTool.PreprocessorDefinitions.Contains("DEBUG") &&
                                 !compilerTool.PreprocessorDefinitions.Contains("NDEBUG"));
                    }
                    if(!debug)
                    {
                        debug = conf.Name.Contains("Debug");
                    }
                    foreach(string component in components)
                    {
                        if(String.IsNullOrEmpty(component))
                        {
                            continue;
                        }
                        Util.addCppLib(linkerTool, component, debug);
                    }
                }
            }
        }

        public static void removeIceCppLibs(Project project)
        {
            Util.removeIceCppLibs(project, new ComponentList(Util.ComponentNames.cSharpNames));
        }

        public static void removeIceCppLibs(Project project, ComponentList components)
        {
            if(!isCppProject(project))
            {
                return;
            }

            VCProject vcProject = (VCProject)project.Object;
            IVCCollection configurations = (IVCCollection)vcProject.Configurations;
            VCProjectEngine vcProjectEngine = (VCProjectEngine)configurations.VCProjectEngine;
            IVCCollection platforms = (IVCCollection)vcProjectEngine.Platforms;
            VCPlatform win32Platform = (VCPlatform)platforms.Item("Win32");
            foreach(VCConfiguration conf in configurations)
            {
                if(conf != null)
                {
                    VCCLCompilerTool compilerTool = 
                        (VCCLCompilerTool)(((IVCCollection)conf.Tools).Item("VCCLCompilerTool"));
                    VCLinkerTool linkerTool = (VCLinkerTool)(((IVCCollection)conf.Tools).Item("VCLinkerTool"));

                    bool debug = false;
                    if(!String.IsNullOrEmpty(compilerTool.PreprocessorDefinitions))
                    {
                        debug = (compilerTool.PreprocessorDefinitions.Contains("DEBUG") &&
                                 !compilerTool.PreprocessorDefinitions.Contains("NDEBUG"));
                    }
                    if(!debug)
                    {
                        debug = conf.Name.Contains("Debug");
                    }

                    foreach(string s in components)
                    {
                        if(s != null)
                        {
                            Util.removeCppLib(linkerTool, s, debug);
                        }
                    }
                }
            }
        }

        public static DTE getCurrentDTE()
        {
           return Connect.getCurrentDTE();
        }
    }
}
