﻿using System;
using System.IO;
using System.Windows;
using Microsoft.Win32;
using Microsoft.WindowsAPICodePack.Dialogs;
using ParticleEditor.Properties;
using ParticleModel;
using Microsoft.VisualBasic;

namespace ParticleEditor
{
    /// <summary>
    ///     Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public static readonly DependencyProperty ModelProperty = DependencyProperty.Register(
            "Model", typeof (EditorViewModel), typeof (MainWindow), new PropertyMetadata(default(EditorViewModel)));

        public MainWindow() {
            InitializeComponent();
#if _DEBUG
            var shit = 1;
#endif
            //PreviewControl.DataPath = Settings.Default.TemplePath;
            PreviewControl.DataPath = "";
            var isValidPath = false;
            if (PreviewControl.DataPath.Length > 0)
            {
                
                try
                {
                    var templeDll = Path.Combine(PreviewControl.DataPath, "temple.dll");
                    if (!File.Exists(templeDll))
                    {
                        MessageBox.Show("The chosen ToEE installation directory does not seem to be valid.\n"
                                        + "Couldn't find temple.dll.",
                            "Invalid ToEE Directory");
                        
                    }else
                    {
                        isValidPath = true;
                    }

                } catch (Exception e)
                {
                    isValidPath = false;
                }
            }
            
            if (PreviewControl.DataPath.Length == 0 || !isValidPath)
            {
                ChooseDataPath();
            }
            Model = new EditorViewModel();
            DataContext = Model;
        }

        public EditorViewModel Model
        {
            get { return (EditorViewModel) GetValue(ModelProperty); }
            set { SetValue(ModelProperty, value); }
        }

        private void NewSystem_OnClick(object sender, RoutedEventArgs e)
        {
            
            var NewSystem = new PartSysSpec();
            var shit = Microsoft.VisualBasic.Interaction.InputBox("Enter system name", "System Name", "");
            NewSystem.Name = shit;

            Model.Systems.Add(NewSystem);
            Model.SelectedSystem = NewSystem;

        }

        private void SaveVideo_OnClick(object sender, RoutedEventArgs e)
        {
            if (Model.SelectedSystem == null)
            {
                MessageBox.Show("Please select a particle system before using this functionality.",
                    "Particle System Required");
                return;
            }

            var ofd = new SaveFileDialog
            {
                AddExtension = true,
                DefaultExt = "mp4",
                Filter = "MP4 Video|*.mp4|All Files|*.*"
            };
            var result = ofd.ShowDialog(this);
            if (result.Value)
            {
                VideoRenderer.RenderVideo(Settings.Default.TemplePath,
                    Model.SelectedSystem,
                    ofd.FileName);
            }
        }

        private void OpenPartSysFile(object sender, RoutedEventArgs e)
        {
            var ofd = new OpenFileDialog {Filter = "Particle System Files|*.tab|All Files|*.*"};
            if (!string.IsNullOrWhiteSpace(Settings.Default.TemplePath))
            {
                ofd.InitialDirectory = Path.Combine(Settings.Default.TemplePath, "data", "rules");
            }
            var result = ofd.ShowDialog(this);
            if (result.Value)
            {
                var file = new PartSysFile();
                file.Load(ofd.FileName);
                Model.Systems = file.Specs;
                Model.OpenedFileName = ofd.FileName;
            }
        }


        private void ChooseDataPath()
        {
            var dialog = new CommonOpenFileDialog
            {
                IsFolderPicker = true,
                InitialDirectory = Settings.Default.TemplePath
            };

            var result = dialog.ShowDialog();
            if (result == CommonFileDialogResult.Ok)
            {
                var templeDll = Path.Combine(dialog.FileName, "temple.dll");
                if (!File.Exists(templeDll))
                {
                    MessageBox.Show("The chosen ToEE installation directory does not seem to be valid.\n"
                                    + "Couldn't find temple.dll.",
                        "Invalid ToEE Directory");
                    return;
                }

                Settings.Default.TemplePath = dialog.FileName;
                Settings.Default.Save();
                PreviewControl.DataPath = dialog.FileName;
            }
        }

        private void PreviewControl_OnConfigureDataPath(object sender, EventArgs e)
        {
            ChooseDataPath();
        }

        private void ChooseDataPath_OnClick(object sender, RoutedEventArgs e)
        {
            ChooseDataPath();
        }

        private void CopySystemToClipboard_OnClick(object sender, RoutedEventArgs e)
        {
            if (Model.SelectedSystem != null)
            {
                var spec = Model.SelectedSystem.ToSpec();
                Clipboard.SetText(spec);
                MessageBox.Show("Copied particle system spec to clipboard.");
            }
        }

        private void DeleteEmitter_Click(object sender, RoutedEventArgs e)
        {
            if (Model.SelectedEmitter != null)
            {
                Model.SelectedSystem.Emitters.Remove(Model.SelectedEmitter);
            }
        }

        private void NewEmitter_Click(object sender, RoutedEventArgs e){
            if (Model.SelectedEmitter != null && Model.SelectedSystem != null)
            {
                var partsysName = Model.SelectedEmitter.ToSpec(Model.SelectedSystem.Name);
                Model.SelectedSystem.Emitters.Add(EmitterSpec.Parse(partsysName));

            }  else
            {
                var NewEmitter = new EmitterSpec();
                Model.SelectedSystem.Emitters.Add(NewEmitter);
            }
        }

        private void RenameEmitter_Click(object sender, RoutedEventArgs e){
            if (Model.SelectedEmitter != null && Model.SelectedSystem != null)
            {
                var shit = Microsoft.VisualBasic.Interaction.InputBox("Enter new name", "Rename Emitter", "em");
                Model.SelectedEmitter.Name = shit;
            }
        }
        
    }
}