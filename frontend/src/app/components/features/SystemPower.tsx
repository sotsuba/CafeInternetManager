import { AlertCircle, RefreshCw } from 'lucide-react';
import { motion } from 'motion/react';
import { useEffect, useState } from 'react';
import { useGateway } from '../../services';
import { HelpButton } from '../HelpButton';
import { Button } from '../ui/button';

interface SystemPowerProps {
  backendId: string;
}

type PowerAction = 'restart' | 'shutdown' | null;

export function SystemPower({ backendId }: SystemPowerProps) {
  const { sendCommand, activeClient } = useGateway();
  const [confirmAction, setConfirmAction] = useState<PowerAction>(null);

  useEffect(() => {
    sendCommand('get_state');
  }, [backendId, sendCommand]);

  const handlePowerAction = (action: PowerAction) => {
    setConfirmAction(action);
  };

  const confirmPowerAction = () => {
    if (!confirmAction) return;
    sendCommand(confirmAction);
    setConfirmAction(null);
  };

  return (
    <div className="h-full flex flex-col p-6">
      <div className="mb-6 flex items-center justify-between">
        <div className="flex items-center gap-3">
          <h3 className="text-2xl">System Power</h3>
          <HelpButton
            title="System Power Management"
            description="Control the remote system's power state. Restart or shutdown the backend device remotely."
            instructions={[
              'Select Restart to reboot the remote system',
              'Select Shutdown to power off the remote device',
              'Confirm your action in the dialog',
              'Commands are sent immediately upon confirmation',
            ]}
          />
        </div>
        <Button variant="outline" size="icon" onClick={() => sendCommand('get_state')}>
          <RefreshCw className="w-5 h-5" />
        </Button>
      </div>

      <div className="flex-1 bg-gray-50 rounded-xl flex items-center justify-center">
        <div className="max-w-2xl w-full p-8">
          <motion.div
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
            className="text-center mb-12"
          >
            <div className="w-24 h-24 bg-yellow-100 rounded-2xl flex items-center justify-center mx-auto mb-6">
              <AlertCircle className="w-12 h-12 text-yellow-600" />
            </div>
            <h4 className="text-xl mb-3">Power Management</h4>
            <p className="text-gray-600">
              Control the power state of the remote device
            </p>
            {activeClient && (
              <p className="text-sm text-gray-500 mt-2">Connected to: {activeClient.hostname}</p>
            )}
          </motion.div>

          <div className="grid grid-cols-2 gap-6 mb-8">
            <motion.button
              whileHover={{ scale: 1.02 }}
              whileTap={{ scale: 0.98 }}
              onClick={() => handlePowerAction('restart')}
              className="bg-white border-2 border-gray-200 rounded-xl p-8 hover:border-blue-500 transition-all text-left group"
            >
              <div className="w-16 h-16 bg-blue-100 rounded-xl flex items-center justify-center mb-4 group-hover:scale-110 transition-transform">
                <svg width="32" height="32" viewBox="0 0 24 24" fill="none">
                  <path
                    d="M23 4v6h-6M1 20v-6h6"
                    stroke="#3B82F6"
                    strokeWidth="2"
                    strokeLinecap="round"
                    strokeLinejoin="round"
                  />
                  <path
                    d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"
                    stroke="#3B82F6"
                    strokeWidth="2"
                    strokeLinecap="round"
                    strokeLinejoin="round"
                  />
                </svg>
              </div>
              <h5 className="mb-2">Restart System</h5>
              <p className="text-sm text-gray-600">
                Reboot the remote device. All unsaved work will be lost.
              </p>
            </motion.button>

            <motion.button
              whileHover={{ scale: 1.02 }}
              whileTap={{ scale: 0.98 }}
              onClick={() => handlePowerAction('shutdown')}
              className="bg-white border-2 border-gray-200 rounded-xl p-8 hover:border-red-500 transition-all text-left group"
            >
              <div className="w-16 h-16 bg-red-100 rounded-xl flex items-center justify-center mb-4 group-hover:scale-110 transition-transform">
                <svg width="32" height="32" viewBox="0 0 24 24" fill="none">
                  <path
                    d="M18.36 6.64a9 9 0 1 1-12.73 0"
                    stroke="#EF4444"
                    strokeWidth="2"
                    strokeLinecap="round"
                    strokeLinejoin="round"
                  />
                  <line x1="12" y1="2" x2="12" y2="12" stroke="#EF4444" strokeWidth="2" strokeLinecap="round" />
                </svg>
              </div>
              <h5 className="mb-2">Shutdown System</h5>
              <p className="text-sm text-gray-600">
                Power off the remote device completely.
              </p>
            </motion.button>
          </div>

          <div className="bg-yellow-50 border border-yellow-200 rounded-xl p-6">
            <div className="flex items-start gap-3">
              <AlertCircle className="w-5 h-5 text-yellow-600 flex-shrink-0 mt-0.5" />
              <div className="text-sm text-yellow-800">
                <p className="font-medium mb-1">Warning</p>
                <p>
                  These actions will immediately affect the remote system. Ensure all work is saved
                  before proceeding. The connection will be lost after shutdown.
                </p>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Confirmation Dialog */}
      {confirmAction && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          className="fixed inset-0 bg-black/50 backdrop-blur-sm z-50 flex items-center justify-center p-6"
        >
          <motion.div
            initial={{ scale: 0.9, opacity: 0 }}
            animate={{ scale: 1, opacity: 1 }}
            transition={{ type: 'spring', duration: 0.3 }}
            className="bg-white rounded-2xl shadow-2xl max-w-md w-full p-8"
          >
            <div className="text-center mb-6">
              <div
                className={`w-16 h-16 rounded-2xl flex items-center justify-center mx-auto mb-4 ${
                  confirmAction === 'restart' ? 'bg-blue-100' : 'bg-red-100'
                }`}
              >
                {confirmAction === 'restart' ? (
                  <svg width="32" height="32" viewBox="0 0 24 24" fill="none">
                    <path
                      d="M23 4v6h-6M1 20v-6h6"
                      stroke="#3B82F6"
                      strokeWidth="2"
                      strokeLinecap="round"
                      strokeLinejoin="round"
                    />
                    <path
                      d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"
                      stroke="#3B82F6"
                      strokeWidth="2"
                      strokeLinecap="round"
                      strokeLinejoin="round"
                    />
                  </svg>
                ) : (
                  <svg width="32" height="32" viewBox="0 0 24 24" fill="none">
                    <path
                      d="M18.36 6.64a9 9 0 1 1-12.73 0"
                      stroke="#EF4444"
                      strokeWidth="2"
                      strokeLinecap="round"
                      strokeLinejoin="round"
                    />
                    <line x1="12" y1="2" x2="12" y2="12" stroke="#EF4444" strokeWidth="2" strokeLinecap="round" />
                  </svg>
                )}
              </div>
              <h4 className="text-xl mb-2">Confirm {confirmAction === 'restart' ? 'Restart' : 'Shutdown'}</h4>
              <p className="text-gray-600">
                Are you sure you want to {confirmAction} the remote system?
                {confirmAction === 'shutdown' && ' You will lose connection.'}
              </p>
            </div>

            <div className="flex gap-3">
              <Button
                onClick={() => setConfirmAction(null)}
                variant="outline"
                className="flex-1"
              >
                Cancel
              </Button>
              <Button
                onClick={confirmPowerAction}
                className={`flex-1 ${
                  confirmAction === 'restart'
                    ? 'bg-blue-600 hover:bg-blue-700'
                    : 'bg-red-600 hover:bg-red-700'
                } text-white`}
              >
                Confirm {confirmAction === 'restart' ? 'Restart' : 'Shutdown'}
              </Button>
            </div>
          </motion.div>
        </motion.div>
      )}
    </div>
  );
}
