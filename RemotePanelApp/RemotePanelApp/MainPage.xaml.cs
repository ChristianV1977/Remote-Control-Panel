using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;
using Microsoft.Maker.Serial;
using Microsoft.Maker.RemoteWiring;
using Microsoft.Maker.Firmata;
using System.Threading.Tasks;
using System.Collections.ObjectModel;
using Newtonsoft.Json;
using Windows.UI.Xaml.Media.Imaging;


// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace RemotePanelApp
{

    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    /// 


    public sealed partial class MainPage : Page


    {

        IStream netWorkSerial;
        RemoteDevice arduino;
        //NetworkSerial netWorkSerial;
        UwpFirmata firmata;

        //public List<menu> menuitems;
        public Target newTarget;

        public MainPage()
        {

            this.InitializeComponent();

            //this.InitializeComponent();
            //MyFrame.Navigate(typeof(Page1));

            ushort port = System.Convert.ToUInt16("3030");

            firmata = new UwpFirmata();
            firmata.StringMessageReceived += Firmata_StringMessageReceived;


            arduino = new RemoteDevice(firmata);
            netWorkSerial = new NetworkSerial(new Windows.Networking.HostName("<your device ip>"), port); //10.0.0.44


            netWorkSerial.ConnectionEstablished += NetWorkSerial_ConnectionEstablished;
            netWorkSerial.ConnectionFailed += NetWorkSerial_ConnectionFailed;
            netWorkSerial.ConnectionLost += NetWorkSerial_ConnectionLost;

            //firmata.begin(netWorkSerial);
            //arduino = new RemoteDevice(netWorkSerial);
            //arduino.DeviceReady += OnDeviceReady;
            //netWorkSerial.begin(115200, SerialConfig.SERIAL_8N1);
            //this.txtStatus.Text = "Connecting..";
            btnEcho.IsEnabled = false;    //ensure all buttons disabled except config and connect
            OnButton.IsEnabled = false;
            OffButton.IsEnabled = false;
            btnConnect.IsEnabled = true;
            btnConnect.IsChecked = false;
            this.txtStatus.Text = "Not Conected";

        }

        public void NetWorkSerial_ConnectionLost(string message)
        {
            //throw new NotImplementedException();
            //this.txtStatus.Text = "Not Connected";
            //btnConnect.IsChecked = false;
            //btnConnect.IsEnabled = false;
            //btnEcho.IsEnabled = false;
            //OnButton.IsEnabled = false;
            //OffButton.IsEnabled = false;


        }


        public void OnDeviceReady()
        {
            var action = Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, new Windows.UI.Core.DispatchedHandler(() => {
                OnButton.IsEnabled = true;
                OffButton.IsEnabled = true;
                btnEcho.IsEnabled = true;
                btnConnect.IsEnabled = true;
                btnConnect.IsChecked = true;

            }));
        }

        public async Task getData(string inverter_command)
        {
            firmata.flush();
            firmata.sendString(inverter_command);

        }


        public void btnConnect_Click(object sender, RoutedEventArgs e)
        {
            btnConnect.IsEnabled = false;                   //prevent another click until conected or disconected
            if (this.txtStatus.Text == "Connected")
            {

                netWorkSerial.end();


            }
            else
            {

                firmata.begin(netWorkSerial);
                //arduino = new RemoteDevice(netWorkSerial);   //??
                arduino.DeviceReady += OnDeviceReady;
                netWorkSerial.begin(9600, SerialConfig.SERIAL_8N1);
                this.txtStatus.Text = "Connecting..";
            }


        }


        public void NetWorkSerial_ConnectionFailed(string message)
        {
            this.txtStatus.Text = string.Format("Connection Failed. {0}", message);
            btnConnect.IsChecked = false;
            btnConnect.IsEnabled = true;

        }



        public void NetWorkSerial_ConnectionEstablished()
        {
            this.txtStatus.Text = "Connected";
            btnEcho.IsEnabled = true;
            OnButton.IsEnabled = true;
            OffButton.IsEnabled = true;

        }


        public void OffButton_Click(object sender, RoutedEventArgs e)
        {
            OffButton.IsEnabled = false;
            OnButton.IsEnabled = true;
            OffButton.IsChecked = true;
            OnButton.IsChecked = false;
            firmata.flush();
            firmata.sendSysex(0x40, new byte[] { 0 }.AsBuffer());



            //var action = Dispatcher.RunAsync(
            //    Windows.UI.Core.CoreDispatcherPriority.Normal,
            //    new Windows.UI.Core.DispatchedHandler(() =>
            //    {
            //        OnButton.IsEnabled = true;
            //        OffButton.IsEnabled = false;

            //    }));

        }
        public void OnButton_Click(object sender, RoutedEventArgs e)
        {
            OnButton.IsEnabled = false;
            OffButton.IsEnabled = true;
            OffButton.IsChecked = false;
            OnButton.IsChecked = true;
            firmata.flush();
            firmata.sendSysex(0x40, new byte[] { 1 }.AsBuffer());


            //var action = Dispatcher.RunAsync(
            //    Windows.UI.Core.CoreDispatcherPriority.Normal,
            //    new Windows.UI.Core.DispatchedHandler(() =>
            //    {
            //        OnButton.IsEnabled = false;
            //        OffButton.IsEnabled = true;

            //    }));

        }




        public void btnEcho_Click(object sender, RoutedEventArgs e)
        {
            btnEcho.IsEnabled = false; //disable button untill restults received ?? timeout needed???
            firmata.flush();
            firmata.sendSysex(0x41, new byte[] { 1 }.AsBuffer());



        }


        public class Target
        {
            public string INP_BAT_VOLT;
            public string BATT_LEVEL_PERC;
            public string OUT_LOAD_W;
            public string OUT_LOAD_PERC;
            public string INP_PV_VOLT;
            public string INP_CHG_AMP;
            public string INP_VOLT;
            public string INP_FREQ;
            public string OUT_LOAD_VA;
            public string OUT_DISC_AMP;
            public string OUT_VOLT;
            public string OUT_FREQ;
            public string MODE_MAINS;
            public string I_SETTING;
            public string CPU_VERSION;
            public string CPU2_VERSION;

        }

        public async void Firmata_StringMessageReceived(
                          UwpFirmata caller, StringCallbackEventArgs argv)
        {

            await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
            {

                string json = argv.getString();

                if (IsValidJson(json))
                {
                    try
                    {
                        newTarget = JsonConvert.DeserializeObject<Target>(json);


                        try
                        {
                            this.lva.Text = newTarget.INP_BAT_VOLT;
                            this.rva.Text = newTarget.BATT_LEVEL_PERC;
                        }
                        catch (Exception ex)
                        {
                            this.lva.Text = "?";
                            this.rva.Text = "?";
                        }


                        if (Int32.Parse(newTarget.BATT_LEVEL_PERC) < 100)
                        { this.lico.Source = new BitmapImage(new Uri(base.BaseUri, "/assets/charged75.png")); }

                        if (Int32.Parse(newTarget.BATT_LEVEL_PERC) < 75)
                        { this.lico.Source = new BitmapImage(new Uri(base.BaseUri, "/assets/charged50.png")); }

                        if (Int32.Parse(newTarget.BATT_LEVEL_PERC) < 10)
                        { this.lico.Source = new BitmapImage(new Uri(base.BaseUri, "/assets/charged25.png")); }

                        if (Int32.Parse(newTarget.BATT_LEVEL_PERC) == 100)
                        { this.lico.Source = new BitmapImage(new Uri(base.BaseUri, "/assets/charged100.png")); }

                        try
                        {
                            this.lva2.Text = newTarget.OUT_LOAD_W;
                            this.rva2.Text = newTarget.OUT_LOAD_PERC;
                        }
                        catch (Exception ex)
                        {
                            this.lva2.Text = "?";
                            this.rva2.Text = "?";
                        }
                        try
                        {
                            this.lva3.Text = newTarget.INP_PV_VOLT;
                            this.rva3.Text = newTarget.INP_CHG_AMP;
                        }
                        catch (Exception ex)
                        {
                            this.lva3.Text = "?";
                            this.rva3.Text = "?";
                        }
                        try
                        {
                            this.lva4.Text = newTarget.INP_VOLT;
                            this.rva4.Text = newTarget.INP_FREQ;
                        }
                        catch (Exception ex)
                        {
                            this.lva4.Text = "?";
                            this.rva4.Text = "?";
                        }
                        try
                        {
                            this.lva5.Text = newTarget.OUT_LOAD_VA;
                            this.rva5.Text = newTarget.OUT_DISC_AMP;
                        }
                        catch (Exception ex)
                        {
                            this.lva5.Text = "?";
                            this.rva5.Text = "?";
                        }
                        try
                        {
                            this.lva6.Text = newTarget.OUT_VOLT;
                            this.rva6.Text = newTarget.OUT_FREQ;
                        }
                        catch (Exception ex)
                        {
                            this.lva5.Text = "?";
                            this.rva5.Text = "?";
                        }


                        btnEcho.IsChecked = false;
                        btnEcho.IsEnabled = true;
                    }
                    catch (Exception ex)                //some exception with json data 
                    {
                        this.txtStatus.Text = "Invalid data";
                    }
                }
                else
                {
                    this.txtStatus.Text = "Invalid data";
                }
            });
        }


        private static bool IsValidJson(string strInput)
        {
            strInput = strInput.Trim();
            if ((strInput.StartsWith("{") && strInput.EndsWith("}")) || //For object
                (strInput.StartsWith("[") && strInput.EndsWith("]"))) //For array
            {
                try
                {
                    // var obj = JToken.Parse(strInput);
                    return true;
                }
                catch (JsonReaderException jex)
                {
                    //Exception in parsing json

                    return false; //false;
                }
                catch (Exception ex) //some other exception
                {
                    return false; //false;
                }
            }
            else
            {
                return false; //false;
            }
        }



        public async void Firmata_SysexMessageReceived(
                          UwpFirmata caller, StringCallbackEventArgs argv)
        {
            var content = argv.getString();
            await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
            {

                btnEcho.IsChecked = false;
                btnEcho.IsEnabled = true;

            });
        }

        private void sbu_click(object sender, RoutedEventArgs e)
        {
            firmata.flush();
            firmata.sendSysex(0x42, new byte[] { 2 }.AsBuffer());
        }

        private void solar_click(object sender, RoutedEventArgs e)
        {
            firmata.flush();
            firmata.sendSysex(0x42, new byte[] { 1 }.AsBuffer());
        }

        private void utility_click(object sender, RoutedEventArgs e)
        {
            firmata.flush();
            firmata.sendSysex(0x42, new byte[] { 0 }.AsBuffer());
        }

        private void btnExit_Click(object sender, RoutedEventArgs e)
        {
            Application.Current.Exit();
        }
    }
}
