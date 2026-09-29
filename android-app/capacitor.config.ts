import type { CapacitorConfig } from '@capacitor/cli';

const config: CapacitorConfig = {
  appId: 'cz.bernytech.gogen',
  appName: 'GoGEN',
  webDir: 'dist',
  android: { backgroundColor: '#0b0f14' },
  plugins: {
    BluetoothLe: {
      displayStrings: {
        scanning: 'Hledám GoGEN…',
        cancel: 'Zrušit',
        availableDevices: 'Dostupná zařízení',
        noDeviceFound: 'Nic nenalezeno',
      },
    },
  },
};

export default config;
