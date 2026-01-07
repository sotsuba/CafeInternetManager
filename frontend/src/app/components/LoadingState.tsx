import { motion, AnimatePresence } from 'motion/react';
import { useState } from 'react';

const loadingMessages = [
  { text: 'Working on the details...', icon: '⚙️' },
  { text: 'Thinking...', icon: '🤔' },
  { text: 'Building your idea...', icon: '🎨' },
  { text: 'Almost there...', icon: '✨' },
  { text: 'Getting everything ready...', icon: '🚀' },
];

interface LoadingStateProps {
  message?: string;
}

export function LoadingState({ message }: LoadingStateProps) {
  const [currentMessageIndex, setCurrentMessageIndex] = useState(0);

  // Cycle through messages
  useState(() => {
    const interval = setInterval(() => {
      setCurrentMessageIndex((prev) => (prev + 1) % loadingMessages.length);
    }, 2500);
    return () => clearInterval(interval);
  });

  const displayMessage = message || loadingMessages[currentMessageIndex];

  return (
    <div className="flex flex-col items-center justify-center gap-6 py-12">
      {/* Animated geometric shapes */}
      <div className="relative w-24 h-24">
        <motion.div
          className="absolute inset-0 border-4 border-black/10 rounded-2xl"
          animate={{
            rotate: [0, 90, 180, 270, 360],
            scale: [1, 1.1, 1, 0.9, 1],
          }}
          transition={{
            duration: 3,
            repeat: Infinity,
            ease: 'easeInOut',
          }}
        />
        <motion.div
          className="absolute inset-3 bg-black rounded-xl"
          animate={{
            rotate: [360, 270, 180, 90, 0],
            scale: [1, 0.9, 1, 1.1, 1],
          }}
          transition={{
            duration: 3,
            repeat: Infinity,
            ease: 'easeInOut',
          }}
        />
        <motion.div
          className="absolute inset-6 bg-white rounded-lg"
          animate={{
            rotate: [0, 90, 180, 270, 360],
          }}
          transition={{
            duration: 2,
            repeat: Infinity,
            ease: 'linear',
          }}
        />
      </div>

      {/* Message */}
      <AnimatePresence mode="wait">
        <motion.div
          key={typeof displayMessage === 'string' ? displayMessage : displayMessage.text}
          initial={{ opacity: 0, y: 10 }}
          animate={{ opacity: 1, y: 0 }}
          exit={{ opacity: 0, y: -10 }}
          transition={{ duration: 0.3 }}
          className="text-center"
        >
          <p className="text-lg text-gray-600">
            {typeof displayMessage === 'string' ? displayMessage : (
              <>
                <span className="text-2xl mr-2">{displayMessage.icon}</span>
                {displayMessage.text}
              </>
            )}
          </p>
        </motion.div>
      </AnimatePresence>
    </div>
  );
}
