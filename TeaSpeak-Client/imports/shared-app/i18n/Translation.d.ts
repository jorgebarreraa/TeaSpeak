export declare abstract class AbstractTranslationResolver {
    private translationCache;
    protected constructor();
    /**
     * Translate the target message.
     * @param message
     */
    translateMessage(message: string): any;
    protected invalidateCache(): void;
    /**
     * Register a translation into the cache.
     * @param message
     * @param translation
     * @protected
     */
    protected registerTranslation(message: string, translation: string): void;
    protected abstract resolveTranslation(message: string): string;
}
