//
//  ViewController.swift
//  TestMac
//
//  Created by Hench on 7/13/20.
//  Copyright © 2020 Hench. All rights reserved.
//

import Cocoa

class ViewController: NSViewController {

    @IBOutlet weak var btnSwitch: NSButton!
    @IBOutlet weak var txtSwitch: NSTextField!
    @IBOutlet weak var txtUserName: NSTextField!
    @IBOutlet weak var txtPasswrod: NSSecureTextField!
    @IBOutlet weak var txtMonth: NSTextField!
    @IBOutlet weak var txtToday: NSTextField!
    @IBOutlet weak var btnLogin: NSButton!
    var isRun = 0;
    var g_premium = 0;
    var out_of_quota = 0;
    var g_day_limit = 0;
    var g_month_limit = 0;
    @IBOutlet weak var txtUserStatus: NSTextField!
    
    
    func hardwareUUID() -> String?
    {
        let matchingDict = IOServiceMatching("IOPlatformExpertDevice")
        let platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict)
        defer{ IOObjectRelease(platformExpert) }

        guard platformExpert != 0 else { return nil }
        return IORegistryEntryCreateCFProperty(platformExpert, kIOPlatformUUIDKey as CFString, kCFAllocatorDefault, 0).takeRetainedValue() as? String
    }
    override func viewDidAppear() {
        super.viewDidAppear()
        self.view.window?.title = "TinyVPN"
    }
    override func viewDidLoad() {
        super.viewDidLoad()
        print(hardwareUUID())
        let defaults = UserDefaults.standard
    
        self.txtUserName.stringValue = defaults.string(forKey: "key_username") ?? ""
        self.txtPasswrod.stringValue = defaults.string(forKey: "key_password") ?? ""
        // Do any additional setup after loading the view.
        /*let format = NumberFormatter()
        format.numberStyle = .decimal
        let string = format.string(from: NSNumber(value: 123456789))
        print(string)*/
    }
    override func viewWillDisappear() {
        if (self.isRun == 0) {
            return
        }
        let service = NetworkService()
        service.stop_vpn(1)
        while (true) {
            if (self.isRun == 0) {
                break
            }
            print("wait for quit...")
            sleep(1)
        }
        super.viewWillDisappear()
    }
    override var representedObject: Any? {
        didSet {
        // Update the view, if already loaded.
        }
    }
    func show_traffic(d : NSInteger, m : NSInteger) {  // 100 M
        let format = NumberFormatter()
        format.numberStyle = .decimal
        out_of_quota = 0;
        let dd = format.string(from: d as NSNumber) ?? ""
        let dl = format.string(from: g_day_limit as NSNumber) ?? ""
        if (g_premium < 2 && g_day_limit != 0 && d > g_day_limit) {
            txtToday.stringValue = "Today: " + dd + " kB. No enough quota, pleaes upgrade to premium user."
            let service = NetworkService()
            service.stop_vpn(1)
            print("out of quota")
            out_of_quota = 1;
        } else {
            if (g_premium < 2) {
                txtToday.stringValue = "Today: " + dd + " kB / " + dl + " kB"
            } else {
                txtToday.stringValue = "Today: " + dd + " kB"
            }
        }
        let mm = format.string(from: m as NSNumber) ?? ""
        let ml = format.string(from: g_month_limit as NSNumber) ?? ""
        if (g_premium >= 2 && g_month_limit != 0 && m > g_month_limit) {  // 10 G
            txtMonth.stringValue = "This month: " + mm + " kB. No enough quota for premium user."
            let service = NetworkService()
            service.stop_vpn(1)
            print("out of quota.")
            out_of_quota = 1;
        } else {
            if (g_premium >= 2){
                txtMonth.stringValue = "This month: " + mm + " kB / " + ml + " kB"
            } else {
                txtMonth.stringValue = "This month: " + mm + " kB"
            }
        }
    }
    @IBAction func loginAction(_ sender: NSButton) {
        if g_premium != 0 {
            self.txtUserName.isEnabled = true
            self.txtPasswrod.isEnabled = true
            self.btnLogin.title = "Login"
            self.txtUserStatus.stringValue = "unregisted user. using your real email to register."
            self.g_premium = 0
            return
        }
        let service = NetworkService()
        let dispatchQueue = DispatchQueue(label: "QueueIdentification", qos: .background)
        let observer = UnsafeMutableRawPointer(Unmanaged.passUnretained(self).toOpaque())
        let username = self.txtUserName?.stringValue
        let password = self.txtPasswrod?.stringValue
        dispatchQueue.async {
            //let locale = Locale.current
            //print("country code:", locale.regionCode)
            service.login(username, pwd: password, device_id: self.hardwareUUID(),
              traffic_call: { (todayTraffic:Int,monthTraffic:Int,dayLimit:Int,monthLimit:Int,ret1:Int,ret2:Int,observer) in
              let mySelf = Unmanaged<ViewController>.fromOpaque(observer!).takeUnretainedValue()
              DispatchQueue.main.async{
                    if (ret1 == 0) {
                        if (ret2 == 1) {
                            mySelf.txtUserStatus.stringValue = "basic user login ok."
                        } else if (ret2==2) {
                            mySelf.txtUserStatus.stringValue = "premium user login ok."
                        }
                        mySelf.g_premium = ret2
                        mySelf.txtUserName.isEnabled = false
                        mySelf.txtPasswrod.isEnabled = false
                        mySelf.btnLogin.title = "Logout"
                        mySelf.g_day_limit = dayLimit
                        mySelf.g_month_limit = monthLimit
                        //btnSubLaunch.setEnabled(true)
                    } else {
                        mySelf.txtUserStatus.stringValue = "login fail."
                        mySelf.txtUserName.stringValue = ""
                        mySelf.txtPasswrod.stringValue = ""
                    }
                    //print("show traffic")
                    mySelf.show_traffic(d: todayTraffic, m: monthTraffic)
              }
          }, withTarget: observer)

        }
        let defaults = UserDefaults.standard
        defaults.set(self.txtUserName.stringValue, forKey: "key_username")
        defaults.set(self.txtPasswrod.stringValue, forKey: "key_password")
    }
    
    @IBAction func switchAction(_ sender: NSButton) {
        if btnSwitch.state == NSControl.StateValue.on {
            if self.isRun == 1 {
                print("not run over")
                return
            }
            print("state on")
            txtSwitch.stringValue = "Connecting"
            
            let service = NetworkService()
            let dispatchQueue = DispatchQueue(label: "QueueIdentification", qos: .background)
            let observer = UnsafeMutableRawPointer(Unmanaged.passUnretained(self).toOpaque())
            let username = self.txtUserName?.stringValue
            let password = self.txtPasswrod?.stringValue
            let locale = Locale.current
            print("country code:", locale.regionCode)
            let country_code = locale.regionCode
            dispatchQueue.async {
                self.isRun = 1
                service.start_vpn(username, pwd: password, device_id: self.hardwareUUID(), premium: self.g_premium,country_code: country_code,
                    stop_call: { (status:Int,observer) in
                    let mySelf = Unmanaged<ViewController>.fromOpaque(observer!).takeUnretainedValue()
                    DispatchQueue.main.async{
                            //0 start vpn succfully, 1 stop vpn
                            if status == 1 {
                                mySelf.txtSwitch.stringValue = "VPN Off"
                                mySelf.isRun = 0
                                print("vpn off")
                                mySelf.btnSwitch.state = NSControl.StateValue.off
                            } else if status == 0 {
                                mySelf.txtSwitch.stringValue = "VPN On"
                                mySelf.btnSwitch.state = NSControl.StateValue.on
                            }
                        }
                    },
                    traffic_call: { (todayTraffic:Int,monthTraffic:Int,dayLimit:Int,monthLimit:Int,observer) in
                        let mySelf = Unmanaged<ViewController>.fromOpaque(observer!).takeUnretainedValue()
                        DispatchQueue.main.async{
                              //print("show traffic")
                            if dayLimit != 0 {
                                mySelf.g_day_limit = dayLimit
                            }
                            if monthLimit != 0 {
                                mySelf.g_month_limit = monthLimit
                            }
                            mySelf.show_traffic(d: todayTraffic, m: monthTraffic)
                        }
                    },
                    withTarget: observer)
                    
                print("thread quit.")
                self.isRun = 0
                //self.txtSwitch.stringValue = "VPN off"
          //      self.ip = String(self.private_ip>>24)+"." + //String(self.private_ip>>16&0xff)+"."+String(self.private_ip>>8&0xff)+"."+String(self.private_ip&0xff)

            }
        } else {
            print("state off")
            let service = NetworkService()
            service.stop_vpn(1)
            //txtSwitch.stringValue = "VPN off"
        }
    }
    
}

